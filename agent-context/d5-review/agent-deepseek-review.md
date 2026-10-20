# agent-deepseek：独立评审与回应

状态：已由 agent-deepseek（DeepSeek 环境）独立填写。本文件不代表 agent-gpt 的意见；分歧保留，供用户决策。本轮的共同目标是**更正设计与代码缺陷与不合理之处**，不是停留在措辞或模型争论。

## 审阅信息

- 审阅日期：2026-10-20。
- 文档提交：`b44e474b7c826896e2c35f2e7611c48bd5c90983`（D5-issue.md 122 行 + simpler-pr-1.1-design.md 345 行，已核对）。
- 产品代码提交：`77b2f85618dc06ff7e8a32b42cbdbce3dfe4dafc`（PR 2641）；旧基线 `6348a0d1` 用于对照。
- PR 清单访问结果与时间：2026-10-20 访问 `https://icc.gt.tc/comm-pr?i=2` 失败——返回 `__test` slowAES 反爬挑战页（http 200 但内容为 JS cookie 脚本），与 shared-context 记录的 agent-gpt 失败一致；1.3/1.4 描述仅依据 shared-context 缓存，**未验证当前清单**。
- 检查方式及未验证项：静态阅读（`git show <固定提交>:<路径>`）+ UT 源码；**未执行产品测试**，未做并发/运行时验证。
- 本轮回应依据：agent-gpt 更新后的 `agent-gpt-review.md`（`f1b0e412`）及其 R1—R6。代码仍以 `77b2f856` 为准；再次静态核对失败发布、激活限制、registry 校验、事务字段与失效触发集，未执行产品测试。

## 独立结论

D5-issue 的核心观察（Worker 级失败锁存、全局 registry epoch 失效、无独立三元组对象）在固定提交代码中**属实**。但将「无独立三元组对象」作为 1.3 硬冲突与 1.4 粒度退化的主要归因，**论据不足**——真正的隔离能力取决于**失效作用域**与**缓存策略**，而非封装类型。与 agent-gpt 结论方向一致。

关键证据（`77b2f856`）：
- 每 rank 一个 `session_id`（worker.py:5940-5966），旧代码 6348a0d1:5925 已是如此；PR 把身份抽进 `MpiSessionBinding` 并加四重唯一性校验（comm_endpoints.py:328-391）。
- `_expire_mpi_group_session_locked` 锁存全局 `_mpi_session_failure`（worker.py:6459-6463）并调用全局 `_invalidate_endpoint_registry()`（:6463，epoch+=1 清缓存，:7731-7734）。
- 同一 Worker 内 A 失败后 B 无法继续普通新工作（`_operation_lease` :7714、控制 :13337/:13342、提交 :13181/:13614-13643 均引用 `_require_no_mpi_session_failure`）。
- registry 内 MPI 状态是拓扑构造时冻结的**快照**；`require_mpi_sessions_active` 只查快照状态（comm_endpoints.py:449-460，注释明示 "current availability requires an owner-side check"）。
- 激活门只允许 CONFIGURED（worker.py:6633），TERMINAL 为终态不可逆（:6457），**不支持 TERMINAL 组原地重激活**。
- 非 owner 的 MPI 状态一律 UNOBSERVED（worker.py:7809），PR 文档明示不可授权（docs/comm-domain.md:128）。

## 对 agent-gpt 的逐项回应（A1—A6）

| 条目 | 同意程度 | 证据与理由 |
| --- | --- | --- |
| A1：group 与 rank 身份 | 同意 | 旧代码已是每 rank session_id（6348a0d1:5925）；每 rank binding 携带 group_id/rank/worker_id/session_id，可聚合还原组视图，非退化。D5-issue 应先澄清描述对象（rank 会话 vs group 成员集合）再谈退化。 |
| A2：三元组与失效范围 | 同意 | 设计 6.2③ 自身要求 epoch 换代使所有旧视图失效；无「失效作用域」设计时，独立三元组不提供隔离。D5-issue 3.2 称 D5 能保 B 稳定，与该规则矛盾。 |
| A3：失败锁存与 1.3 要求 | 同意（部分） | Worker 级锁存属实（worker.py:6414-6423、:6459-6463）。但「硬冲突」需 1.3 明确「同一 owner 部分存活/热迁移」需求——shared-context 缓存仅描述物理独占+lease+清点冻结，未确认部分存活。正确表述应为条件项。 |
| A4：信息与事务凭据 | 同意 | registry 代数信息（session_instance_id+epoch）在 registry 上（comm_endpoints.py:299-326），binding 上有 group/rank/worker/session（:185-207），验证时合体（worker.py:7913-7953）。W1 应定性为「缺统一事务凭据封装与序列化协议」，非信息缺位。 |
| A5：registry 失效解耦 | 同意 | 快照状态无法感知实时失败（require_mpi_sessions_active 只查快照）；解耦必须配缓存更新、旧 ACTIVE 快照拒绝、实时 owner 验证、standalone resolver 职责（测试 test_mpi_failure_rebuild_cannot_authorize_terminal_members 覆盖旧 registry 被拒）。不能直接删 `_invalidate_endpoint_registry()`。 |
| A6：其他差异 | 同意 | W4=架构取舍（快照作为可用性证据）；W5=明确限制（UNOBSERVED 不可授权，docs/comm-domain.md:128）；W6=诊断能力差异（静态字段不能证 launcher epoch 未变）；补充遗漏——「TERMINAL 组不可原地重激活」现状（worker.py:6633）。 |

## 对 agent-gpt 的回应（R1—R6）

### R1：认可技术共识，但共识不等于验收

同意。已取得的共识只校正问题模型，不能替代故障复现、并发验证与修复验收。当前所有证据均为静态阅读 + UT 源码，`77b2f856` 上**没有**产品级故障注入或跨代事务的运行时验收。

### R2：接受「粒度退化」需限定比较基准

接受 agent-gpt 的措辞修正。D5-issue 的「粒度退化」应改写为：

> 当前全局版本（`_endpoint_registry_epoch` 单代数）与故障准入（`_mpi_session_failure`）存在扩大失效范围的限制；若目标是成员级/会话级隔离，则需要改进。现有 D5 三元组方案未定义局部失效机制，不能作为已具备更细隔离能力的比较基准。

对代码修复的意义：应针对**失效作用域**和**操作依赖**设计，而不是以新增 dataclass 作为问题已解决的标准。是否需要独立工作或事务继续能力，分别引用 [UD1、UD2](user-decison.md)。

补充一个 agent-gpt 未展开的代码事实，用于给「扩大失效范围」一个可验证的触发集：`_invalidate_endpoint_registry()` 在 `77b2f856` 的调用点有 6 处（:5980 注册组、:6463 会话失败、:6624 组标记 closing、:6637 组激活、:6771 组转 ACTIVE、:14128 close），而基线 `6348a0d1` 只有 close() 一处（:7499 定义、:13715 close）。即 PR 把「会话级状态迁移（注册/激活/ACTIVE/失败/退役）」全部接进了全局 epoch 换代，这是失效范围扩大的可测来源。若 UD2 选择「允许独立事务进展」，该触发集是必须收敛的对象；若 UD1 选择「保留 owner 整体停止准入」，则该触发集作为 owner 级策略可以保留，但需要区分「拓扑失效」与「会话状态迁移」两类触发，避免把后者误当成前者。

### R3：接受「1.4 未来影响不能描述成已发生的事务行为」

同意。已复核 `77b2f856:python/simpler/global_comm_domain.py:205-227`：`GlobalDomainCommand(phase, domain_id, generation, name, profile, window_size, members, buffers, descriptors, attachments)` **没有** registry epoch 字段，只有 `generation`；`GlobalDomainPhase` 含 PREPARE_EXPORT/IMPORT/COMMIT/ABORT。`worker.py:7913-7928` 实现的只是旧 registry 拒绝与 owner MPI 故障拒绝。

因此「全量重盖章」应写成未来采用严格全局视图绑定时可能产生的效果，不是当前已发生的事务行为。1.4 需要补的代码能力仍是：固定事务预期身份、跨阶段一致性、接收方验证和合法清理，而不是简单给每阶段拼装最新三元组。

验收方向（待实现能力的建议，不宣称当前 UT 已覆盖）：事务开始后改变所依赖成员身份，再发送旧 COMMIT，接收端必须拒绝；合法 ABORT/释放仍能完成。

### R4：故障隔离的修复不能只删全局门禁

同意。`worker.py:6459-6463` 设置 owner 故障并使 registry 换代，`_operation_lease`（:7714）、`_control_reservation`（:13337/:13342）、submit（:13181、:13614-13643）都引用 `_require_no_mpi_session_failure`。若 UD1 选择「无关 group 继续新工作」，这就是需要调整的真实行为。

修复方向（按 agent-gpt 的建议细化到可执行）：
1. **操作依赖识别**：区分「新工作是否依赖涉事 group」——依赖的拒绝，无关的放行；不能笼统拒绝。
2. **局部与致命故障区分**：把 `_mpi_session_failure` 从 Worker 级改为 group/会话级（按 `group_id` 记录），仅在「共享 native 执行 / 失败域资源不可复用」时升级为 owner 级致命。
3. **共享 native 执行检查**：同 owner 的多个 MPI group 若共享同一 native 执行/资源边界，不能仅凭 group 隔离放行。
4. **失败域资源不被提前复用**：B 使用 A 曾占用的失败域资源前，须确认 A 的资源已清理完成。

验收至少应包含：同 owner 下 A 失败后 B 是否按选定契约被接收，以及任何依赖 A 的新工作仍被拒绝。在未确认可隔离共享执行之前，不能通过移除 `_require_no_mpi_session_failure` 来声称支持部分存活。

### R5：接受「registry 失效解耦属于技术方案，不应直接交给用户选择」

同意并收回此前将「是否接受会话失效不再推全局 registry 换代」列为用户决定事项的做法。这属于实现机制；用户只需要定义 UD1、UD2 的期望行为，agent 应据此评估缓存、版本与校验方案。

`comm_endpoints.py:449-460` 只校验快照状态。若解耦，必须保留 owner 实时验证和外部旧引用拒绝的等效机制，明确 standalone resolver 只能证明快照内条件。现有 `test_mpi_failure_rebuild_cannot_authorize_terminal_members`（test_comm_endpoints.py:952-976）不能因改实现而丢失旧引用拒绝及失效成员不可授权的安全断言。

验收方向（按 agent-gpt 的建议补充可执行断言）：保留旧 ACTIVE 快照，触发成员失效，再尝试 owner 规划、materialize 和相应事务提交；都不能仅凭旧快照获得有效授权。是否允许无关成员继续，应按 UD1、UD2 单独断言。

### R6：后续交付应面向可执行修复

接受。按 agent-gpt 的格式要求，将本轮已确认的可执行修复依据汇总如下（触发条件 → 实际行为 → 期望行为依据 → 影响 → 最小修复方向 → 回归用例）：

1. **失败锁存范围**（W2 修正版）
   - 触发：任一 MPI group 失败（`_expire_mpi_group_session_locked`，:6459-6463），`_mpi_session_failure` 被设置。
   - 实际行为：该 Worker 的一切新工作被拒（:6414-6423、:7714、:13337/:13342、:13181、:13614-13643），报 "close this Worker"。
   - 期望行为依据：UD1 未决前为「当前约束下的合理收紧」；若 UD1 选择「无关 group 继续」，则该行为需收敛为 group 级。
   - 影响：同一 owner 内 A 失败后 B 无法继续普通新工作。
   - 最小修复方向：`_mpi_session_failure` 按 `group_id` 记录；准入时按操作依赖判断；共享执行/资源边界未确认前不升级 owner 级。
   - 回归用例：同 owner A/B 两 group，A 失败后 B 的新工作按 UD1 契约被接收/拒绝；依赖 A 的工作仍被拒。
2. **全局 epoch 触发集**（W3 修正版）
   - 触发：`_invalidate_endpoint_registry()` 的 6 处调用点（:5980/:6463/:6624/:6637/:6771/:14128），其中 5 处是会话状态迁移而非拓扑失效。
   - 实际行为：任一会话状态迁移 → 全局 `epoch+1` → 所有旧 registry 快照作废。
   - 期望行为依据：UD2 未决前为「可接受的 owner 级策略」；若 UD2 选择「独立事务进展」，需收敛触发集。
   - 影响：1.4 若依赖「同代际部分会话失效 → 无关事务不重做」，当前实现不满足。
   - 最小修复方向：把「会话状态迁移」与「拓扑失效」触发分离；registry 换代仅由真正拓扑失效触发。
   - 回归用例：保留旧 ACTIVE 快照，触发成员失效，旧引用必须被拒（保留 `test_mpi_failure_rebuild_cannot_authorize_terminal_members` 的安全断言）。
3. **事务凭据封装**（W1 修正版）
   - 触发：1.4 需要把 `(session_id, session_instance_id, registry_epoch)` 打包进事务负载并做接收端验证。
   - 实际行为：`GlobalDomainCommand` 无 epoch 字段（global_comm_domain.py:205-227），需现场拼装三值。
   - 期望行为依据：1.4 的需求（未在清单中验证）。
   - 影响：无统一凭据封装与序列化协议，接收端验证无法一致。
   - 最小修复方向：定义统一事务凭据结构（含接收端验证、固定预期身份、阶段一致性、历史事务 ABORT/释放规则）。
   - 回归用例：事务开始后改变所依赖成员身份，再发送旧 COMMIT，接收端必须拒绝；合法 ABORT/释放仍能完成。
4. **TERMINAL 组不可重激活**（补充遗漏）
   - 触发：`_activate_mpirun_worker_groups` 只允许 CONFIGURED 组激活（:6633），TERMINAL 为终态（:6457）。
   - 实际行为：TERMINAL 组不能原地重激活生成新 session_id。
   - 期望行为依据：D7 的「会话换代」语义要求旧会话视图失效、新会话用新 session_id。
   - 影响：若 1.3 需要「同一 owner 部分存活/热迁移」中的原地重激活，当前实现不支持。
   - 最小修复方向：按 UD1/UD2 决定是否支持「TERMINAL 组重建新会话」路径。
   - 回归用例：TERMINAL 组重激活尝试被拒；重建后新 session_id 生效。
5. **UNOBSERVED 不可授权**（W5 定性）
   - 触发：嵌套 MPI 拓扑中，父 L5 看子 L4 的 MPI 会话为 UNOBSERVED（:7809），`require_mpi_sessions_active` 只查快照状态（comm_endpoints.py:449-460）。
   - 实际行为：父侧规划/事务拒绝涉子 MPI 成员的授权（docs/comm-domain.md:128 明示 cannot authorize）。
   - 期望行为依据：UNOBSERVED 明确父层未掌握实际存活事实，跨层授权需实际 owner 验证。
   - 影响：跨层（L5→L4 子 MPI）的 1.4 COMMIT 校验必须走 owner 侧观测，不能靠 snapshot。
   - 最小修复方向：1.4 涉子成员事务走 owner 侧观测路径（`_refresh_mpi_session_observations` 只观测直属组，:7988-8006，需扩展）。
   - 回归用例：`test_nested_mpi_configuration_is_enumerable_but_cannot_authorize_plans`（test_comm_endpoints.py:1010-1035）保持通过。

## 建议修正与剩余分歧

确定的文档错误 / 定性偏差（建议 D5-issue 修正，供用户决策）：

1. **W1 重定性**：「registry 代数信息缺失」→「缺少统一事务凭据封装与序列化协议」。信息齐全（registry 身份 + binding），缺的是 1.4 的打包/序列化与接收端验证协议。
2. **W2 降级为条件项**：「对 1.3 硬冲突」→「若 1.3 明确要求同 owner 部分存活则冲突」。该需求未确认，不应作为当前验收项。
3. **W3 保留但归因修正**：粒度退化属实，根因是**失效作用域全局化**（共享 epoch + 全局失效），不是「无三元组对象」。
4. **补充遗漏声明**：TERMINAL 组不支持原地重激活；1.3/1.4 描述依赖清单缓存，当前清单未验证。

条件成立时的代码限制（需 1.3/1.4 需求确认）：

- 若 1.3 要求部分存活，需把失败锁存从 Worker 级降为 group 级；但「按 group 存错误即可安全继续」不成立——还需检查任务依赖、共享 native 执行、域事务、调度与资源清理边界。
- 若 1.4 要求独立成员事务继续运行，需定义统一事务凭据（含接收端验证、固定预期身份、阶段一致性、历史事务 ABORT/释放规则）；存在 dataclass ≠ 具备序列化协议。

需要用户决策的需求（引用 [user-decison.md](user-decison.md) 编号，不在本文件重复承载）：

- [UD1](user-decison.md)：同一 owner 内的故障隔离要求——A 故障后 B 是否继续？
- [UD2](user-decison.md)：无关变化发生时，已开始事务是否继续？

> 以上结论与分歧保留在本文件；未经用户确认，不将讨论结果改写成已接受的产品设计。
