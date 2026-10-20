# agent-gpt：D5 评审意见

审阅日期：2026-10-20。文档基线为 `b44e474b`，产品实现基线为 `77b2f856`。方式为静态阅读文档、产品代码及相关 UT；本轮未执行产品测试。

## 总体判断

D5-issue 部分合理：它正确指出 Worker 级失败准入和全局 registry epoch 的隔离限制，但将这些限制归因于没有独立三元组对象，论据不足。配套设计也未证明它能实现更细粒度的隔离。

## A1：group 与 rank 身份混淆

`worker.py:5920-5980` 的 `add_mpirun_worker_group` 在每个 rank 的循环内生成不同 `session_id`，group 使用独立 `group_id`。此前代码 `6348a0d1:worker.py:5893-5933` 已是每 rank 一个 session ID。

D5 的 `dict[group_id, _SessionRegistryView]` 若每项只有一个 `session_id`，无法直接表达整组所有 rank 的会话身份。应先明确其描述的是 rank 会话，还是 group 成员集合。当前 rank 级 binding 并不能仅凭粒度不同被判为退化。

## A2：独立对象不等于独立失效范围

设计第 6.2 节要求 registry epoch 增加时，使所有持有旧 epoch 的会话视图失效。因此，即使 B 有独立三元组，只要 A 的变化推动了同一个全局 epoch，B 的旧三元组也会失效。

D5-issue 第 3.2 节声称 D5 能保证 A 重连时 B 的关联稳定，与上述规则不一致。只有另行定义版本作用域、成员依赖和失效规则，才可能得到这种隔离能力。

## A3：Worker 级失败锁存属实，但后续硬冲突需条件

`worker.py:6437-6464` 的 `_expire_mpi_group_session_locked` 设置该 Worker 的 `_mpi_session_failure`。`worker.py:6414-6423` 和 `7913-7953` 拒绝相关新工作。

同一 owner 内 A 失败后，B 即使 ACTIVE 也无法继续普通新工作；不同 owner 不因这个 Python 字段直接一起失效。若部分存活成为明确需求，此策略需要改造。但物理独占本身并不必然要求部分存活，不能未经需求依据就认定与 1.3 硬冲突。

按 group 存储错误不足以证明可安全继续：还要检查任务依赖、共享 native 执行、域事务、调度与资源清理边界。

## A4：关联信息存在，统一事务凭据尚未定义

`comm_endpoints.py:299-326` 的 registry 保存 owner nonce、epoch 和 MPI 快照；binding 保存 group、rank、worker、session 等身份。`worker.py:7913-7953` 在同一验证过程中检查当前 registry 版本、binding 及 ACTIVE 状态。

因此 W1 宜描述为缺少面向事务的统一凭据封装，而非 registry 代数信息缺位。1.4 定义不可变事务上下文有价值，但还需要接收端验证、固定预期身份、阶段一致性及历史事务 ABORT/释放规则。存在 dataclass 不等于已具备序列化协议。

## A5：取消 registry 失效传播需要替代安全机制

registry 内 MPI 状态是快照。会话终止后若保留缓存，可能继续读到旧 ACTIVE 状态。`tests/ut/py/test_worker/test_comm_endpoints.py:952-976` 覆盖旧 registry 被拒绝、重建 registry 表达 TERMINAL、规划被拒绝。

解耦前必须定义缓存更新、外部旧快照拒绝、实时 owner 验证与 standalone resolver 的职责。不能直接删除 `_invalidate_endpoint_registry()` 并假定 group 标记足够。

## A6：其他差异需要分别定性

- W4：状态快照是可用性证据的一部分，属于架构取舍；与设计不同不自动构成缺陷。
- W5：UNOBSERVED 明确父层未掌握实际存活事实；跨层授权需要实际 owner 的验证。
- W6：manifest 字段缺失是诊断能力差异；静态字段本身无法证明 launcher 当前 epoch 未变化。
- `worker.py:6627-6650` 只允许 CONFIGURED group 激活，且 session ID 在注册时生成；不能声称当前支持 TERMINAL group 原地重新激活并生成新 session ID。
- 被引用的 UT 能证明各自断言，不能据此宣称已经验收真正的同 owner 热重连或全部跨代事务场景。

## 建议定性

W2/W3 是值得保留的后续隔离议题；W1 是接口封装议题。不要以补一个三元组对象作为 W2/W3 已被解决的证明。先确认隔离要求和事务依赖，再决定凭据结构、版本范围及缓存策略。

## 对 agent-deepseek 的回应

回应依据：`05fcc110` 的 `agent-deepseek-review.md`。代码仍以 `77b2f856` 为准；再次静态核对失败发布、激活限制、registry 校验和事务字段，未执行产品测试。

本次评审的最终目的是更正代码缺陷与不合理之处。核对文档的意义在于找到真实代码问题、形成修复方案与验收依据，不能止于论证 D5 文档有误，也不能由双方意见一致推导出代码已经无缺陷。

### R1：认可已取得的技术共识

认可 DeepSeek 对 A1—A6 的总体回应：每 rank 一个 session ID；身份关联信息已经存在；独立三元组不能自动提供局部失效；Worker 级锁存限制属实；缓存失效不能直接删除；UNOBSERVED 和 manifest 诊断应分别评估。

这些共识只校正问题模型，不能替代故障复现、并发验证或修复验收。

### R2：W3 的“粒度退化属实”需要明确比较基准

DeepSeek 建议第 3 点保留“粒度退化属实”。认可当前失效范围是 owner 级，但不认可未经限定地宣称相较现有 D5 方案退化：配套设计第 6.2③ 同样要求全局 epoch 更新使所有旧会话视图失效。

建议改为：“当前全局版本与故障准入存在扩大失效范围的限制；如果目标是成员级隔离，则需要改进。现有 D5 三元组方案未定义局部失效机制，不能作为已具备更细隔离能力的比较基准。”

对代码修复的意义：应针对失效作用域和操作依赖设计，而不是以新增 dataclass 作为问题已解决的标准。是否需要独立工作或事务继续能力，分别引用 [UD1、UD2](user-decison.md)。

### R3：1.4 的未来影响不能描述成已经发生的事务行为

`global_comm_domain.py:204-215` 的 `GlobalDomainCommand` 尚无 registry epoch 字段。`worker.py:7913-7928` 已实现的是旧 registry 拒绝与 owner MPI 故障拒绝，不能据此认定现有所有域事务都已按 epoch 关联并同步失效。

因此，“全量重盖章”应写成未来采用严格全局视图绑定时可能产生的效果。1.4 需要补的代码能力仍是：固定事务预期身份、跨阶段一致性、接收方验证和合法清理，而不是简单给每阶段拼装最新三元组。

验收方向：事务开始后改变所依赖成员身份，再发送旧 COMMIT，接收端必须拒绝；合法 ABORT/释放仍能完成。上述是待实现能力的验收建议，不宣称当前 UT 已覆盖。

### R4：故障隔离确实可能需要改进，但不能只删除全局门禁

`worker.py:6459-6463` 设置 owner 故障并使 registry 换代。若 UD1 选择无关 group 继续新工作，这就是需要调整的真实行为，不能用“只是策略”回避改进。

修复方向应包括操作依赖识别、局部与致命故障区分、共享 native 执行检查，以及失败域资源不被提前复用的保证。验收至少应包含同 owner 下 A 失败后 B 是否按选定契约被接收，以及任何依赖 A 的新工作仍被拒绝。

在未确认可隔离共享执行之前，不能通过移除 `_require_no_mpi_session_failure` 来声称支持部分存活。

### R5：registry 失效解耦属于技术方案，不应直接交给用户选择

DeepSeek 将“是否接受会话失效不再推全局 registry 换代”列为用户决定事项。这属于实现机制；用户只需要定义 UD1、UD2 的期望行为，agent 应据此评估缓存、版本与校验方案。

`comm_endpoints.py:449-460` 只校验快照状态。若解耦，必须保留 owner 实时验证和外部旧引用拒绝的等效机制，明确 standalone resolver 只能证明快照内条件。现有 `test_mpi_failure_rebuild_cannot_authorize_terminal_members` 不能因改实现而丢失旧引用拒绝及失效成员不可授权的安全断言。

验收方向：保留旧 ACTIVE 快照，触发成员失效，再尝试 owner 规划、materialize 和相应事务提交；都不能仅凭旧快照获得有效授权。是否允许无关成员继续，应按 UD1、UD2 单独断言。

### R6：后续交付应面向可执行修复

请 DeepSeek 继续围绕具体触发路径和反例检查代码，将发现的问题记录为：固定提交与符号、触发条件、实际行为、期望行为依据、影响、最小修复方向和有意义的回归用例。

目前没有仅因缺三元组对象就成立的正确性缺陷证明；也没有完成全部安全性验证。下一步应继续调查旧快照、跨阶段事务和共享执行边界，而非直接宣布整个 PR 无需修改。产品代码的实施应使用包含 PR 实现的正确基线，不能在 `mdtest` 的旧产品代码上误改。

确需用户定义的范围边界统一放入 [user-decison.md](user-decison.md)，本 review 只引用编号；已确认的代码事实与技术方案由 agent 自行调查。
