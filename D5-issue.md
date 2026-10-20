# D5-issue：会话↔registry 视图三元组的设计缺口与影响评估

> 关联设计文档：`simpler-pr-1.1-design.md`（PR 1.1 设计，D1-D7 决策）。
> 评估对象：设计文档 **D5 决策**（registry 视图代数 = `(session_id, session_instance_id, registry_epoch)` 三元组）在 PR 2641（`mpi-session-registry` @ `77b2f856`）落地时**未被实现为独立三元组对象**，以及由此对 1.3 / 1.4 造成的影响与缺陷。
> 基线：simpler `main` `6348a0d1`（2026-10-20 分支 mdtest 时点）。

---

## 1. D5 设计意图（设计文档 5.1 / 5.2 / 6.2）

D5 定义 registry 视图代数 = 一次会话的「代数快照」：

```text
@dataclass(frozen=True)
class _SessionRegistryView:          # D5 三元组
    session_id: int                  # 会话代数锚点（worker.py:5961 uuid）
    session_instance_id: bytes       # registry nonce（= launcher._owner_instance_id）
    registry_epoch: int              # registry 拓扑代数（launcher._endpoint_registry_epoch）

self._session_registry_views: dict[group_id, _SessionRegistryView]  # 每会话一条
```

设计意图（9.1）：

- **对 1.3**：*「1.3 的『部分存活/热迁移』正是 2.3 列出的『独立会话 epoch 有意义』的场景之一；1.1 的 D5 三元组是 1.3 区分『同一 worker 两个会话化身』的代数基础」*
- **对 1.4**：*「1.4 补 registry epoch 关联 = 用本 PR D5 的锚点扩展事务负载，不在帧头」*

即：**会话级、可独立引用、含 registry 代数的不可变三元组对象**，供下游 1.3/1.4 直接复用。

---

## 2. PR 2641 的实际实现形态（与设计的差异）

PR 2641 未实现 `_SessionRegistryView` 三元组，改用「rank 级 binding + 会话状态机 + 全局 epoch 失效缓存」：

```text
MpiSessionBinding(owner_path, group_id, rank, worker_id, session_id)   # frozen，每 rank 一条
MpiSessionSnapshot(binding, state, failure_reason)                      # 状态快照，投影进 EndpointRegistry
_MpiL3GroupRuntime.state: CONFIGURED→STARTING→ACTIVE→RETIRED/TERMINAL   # 会话状态机
_mpi_session_failure: BaseException | None                              # 全局锁存首个失败，永不解除
```

| 维度 | 设计 D5 | PR 2641 实现 |
| --- | --- | --- |
| 载体 | 独立 `dict[group_id, _SessionRegistryView]` | binding 存于 rank + 投影进 EndpointRegistry |
| 粒度 | 每 group 一条 | 每 rank 一条 |
| 字段 | `(session_id, session_instance_id, registry_epoch)` 三元组齐全 | `(owner_path, group_id, rank, worker_id, session_id)`，**registry 代数缺位** |
| epoch 换代 | 会话内视图换代，命中并标记旧视图 | 全局 epoch+1 废弃整个缓存，会话状态不动 |
| 失效粒度 | 会话级标记 | **全局锁存** `_mpi_session_failure` + 全局 `_invalidate_endpoint_registry()` |
| 状态位置 | launcher 侧记录，registry 只加可选只读诊断字段 | 状态内嵌进 registry 每个 MPI 端点，参与判定（`require_mpi_sessions_active`） |
| 校验 | `_validate_session_registry_view(group_id)` 查表比三元组 | `_validate_endpoint_registry_snapshot` 逐 member 比对 binding 全等 + ACTIVE + registry 身份一致 |
| 代数锚点对象 | 有独立不可变对象 | **无独立对象**，需从 registry 身份 + binding.session_id 现场拼装 |

关键实现代码（worker.py patch）：

```python
# _expire_mpi_group_session_locked —— 全局锁存，只记首个失败，永不解除
if self._mpi_session_failure is None:
    summary = RuntimeError(f"MPI group {group.group_id} {kind.value} is unavailable")
    self._mpi_session_failure = summary
self._invalidate_endpoint_registry()   # 全局 epoch+1，废弃整个 registry 缓存

# _require_no_mpi_session_failure_locked —— 挂在 operation_lease/submit/control/materialize 上
raise RuntimeError(f"Worker.{api}: MPI session is unavailable; close this Worker")
```

---

## 3. D5 影响的评估（对 1.3 / 1.4）

### 3.1 对 1.3（物理设备独占与能力清点冻结，dep 1.1）的影响

1.3 需要：解决「同一物理卡被两个 session 声明」；区分「同一 worker 的两个会话化身」；支持「部分存活 / 跨会话存续」。

| 1.3 需要 | 设计 D5 提供 | PR 2641 提供 | 判定 |
| --- | --- | --- | --- |
| (a) 会话级身份可索引（device→session 归属表） | 每 group 一条三元组 | binding 有 `session_id`（每 rank） | ✅ 两者都有，独占表可用 session_id 索引 |
| (b) 区分「同一 worker 两个会话化身」 | 三元组（会话代数锚点） | binding 同时带 `worker_id` + `session_id`，重激活后 session_id 变→可区分 | ✅ 功能上够 |
| (c) **一个会话挂了，其他会话继续活**（部分存活） | 会话级失效：关会话只标记/移除**该会话**视图（6.2②），其他视图不动 | **首个失败锁存全局 `_mpi_session_failure`，该 Worker 永久拒绝一切新工作（"close this Worker"）** | ❌ **硬冲突** |

**结论**：(a)(b) 身份信息够用；(c) 是核心矛盾——PR 的全局锁存模型下，**会话 A 失败 → 会话 B 的所有 submit/control/materialize 也全被拒**。在 1.1 的「固定成员、整组重启」约束下这是合理收紧，但 1.3 的「部分存活/跨会话保留部分 worker」恰好需要「B 不陪葬」。**1.3 开工时第一优先改造点**：把 `_mpi_session_failure` 从 Worker 级降到 group 级（或按 group 记录、只拒绝涉事会话的准入）。

> 助力点：`UNOBSERVED` 状态对 1.3 反而是正向——嵌套（父 L5 看子 L4）MPI 会话不可授权计划，物理独占检查跨层级时不会被 UNOBSERVED 会话的端点冒充授权。

### 3.2 对 1.4（成员事务的会话/epoch 关联与重连验证，dep 1.1·1.3）的影响

1.4 要做：「四阶段与全员 IMPORT 后 COMMIT 已有，不重做，只补域 `generation` 之外的 registry epoch 关联」。`GlobalDomainCommand` 目前只带 `generation`（global_comm_domain.py:204-215）。

| 1.4 需要 | 设计 D5 提供 | PR 2641 提供 | 判定 |
| --- | --- | --- | --- |
| (a) 事务负载里携带 `(session_id, session_instance_id, registry_epoch)` | **现成的三元组对象，直接序列化进命令负载**（设计明说"用 D5 锚点扩展事务负载"） | **无对象**，但三值都在 launcher 上：`binding.session_id` + `registry.session_instance_id` + `registry.registry_epoch` | ⚠️ 数据够，1.4 需自己定义打包/序列化结构 |
| (b) COMMIT 校验「事务的 epoch 关联 vs 会话当前视图」 | 三元组直接比对 | 需现场拼装后比对 | ⚠️ 可做，多一层拼装 |
| (c) **重连验证**：一个会话重连不波及其他会话的关联 | 会话级视图，B 的关联在 A 重连时保持稳定 | 任何 group 失效都 `_invalidate_endpoint_registry()` 全局 epoch+1 → **所有域事务的关联同时过期，需全量重盖章** | ⚠️ 粒度退化 |
| (d) 跨层事务（L5→L4 子 MPI 成员）的 COMMIT 校验 | 设计未展开（5.4 只提示 rank 侧身份是它自己的 nonce） | 子会话状态 UNOBSERVED，父侧 snapshot **不能授权**；`_refresh_mpi_session_observations` 只观测**直属** group | ⚠️ 1.4 若涉子成员，COMMIT 校验必须走 owner 侧观测，不能靠 snapshot |

**结论**：身份三值**都在**，1.4 可自造打包结构，但 (c) 粒度退化需要额外处理——PR 把**任一会话失败**也当成全局 registry 换代触发源，导致「同代际内部分会话失效 → 全量重盖章」这种设计里不存在的情况。1.4 的重连验证逻辑必须兼容它。

---

## 4. 缺点清单（汇总）

| # | 缺点 | 影响面 | 严重度 | 触发条件 |
| --- | --- | --- | --- | --- | --- |
| W1 | **无独立三元组锚点对象**：registry 代数（`session_instance_id`/`registry_epoch`）不在会话身份载体上，下游无法直接引用会话级代数 | 1.3 / 1.4 | 中 | 1.3/1.4 需要会话级代数身份时 |
| W2 | **全局锁存 `_mpi_session_failure` 永久生效**：首个 MPI 会话失败后该 Worker 拒绝一切新工作，`"close this Worker"` | 1.3（部分存活不可能） | **高** | 单会话失败、其他会话仍 ACTIVE |
| W3 | **任一会话失效触发全局 registry 换代**：`_expire_mpi_group_session` / `_mark_mpirun_groups_closing` 都调 `_invalidate_endpoint_registry()`，单会话挂→整个 registry 缓存废弃重建（epoch+1） | 1.4（重连验证全量重盖章）、性能（频繁重建） | 中高 | 单会话失效时 |
| W4 | **状态内嵌 registry 参与判定**：`MpiSessionSnapshot` 进入 `EndpointRegistry`，registry 构造不再纯拓扑——standalone `BackendResolver` 依赖内嵌状态，与设计「registry 只加可选只读诊断字段」不符 | 解耦/复用 | 低中 | 无 worker 存活的 standalone 规划场景 |
| W5 | **跨层（嵌套）会话不可授权**：父 L5 看子 L4 的 MPI 会话为 UNOBSERVED，父侧规划/事务一律拒绝涉子 MPI 成员的授权 | 1.3（跨层独占检查）、1.4（跨层 COMMIT） | 低中（设计未展开，属新增语义） | 嵌套 MPI 拓扑参与规划 |
| W6 | **无 manifest registry_view 自检**（D4 未落地）：rank 侧 inner_worker 无法自检「launcher 声明的 registry 视图」，诊断能力缺失 | 诊断/可观测 | 低 | rank 侧需要会话归属诊断时 |

---

## 5. 结论与建议

1. **对 1.1 自身验收**：D5 形态差异不阻断——D7 的「会话换代 / registry 换代互相失效、跨会话复用拒绝」在 PR 中均已实现并经测试覆盖（`test_stale_failure_event_cannot_expire_current_group`、`test_mpi_failure_rebuild_cannot_authorize_terminal_members`、`test_nested_mpi_configuration_is_enumerable_but_cannot_authorize_plans`）。
2. **对 1.3**：W2 是硬冲突，1.3 开工前必须把失败锁存从 Worker 级降为会话/group 级。
3. **对 1.4**：W3 粒度退化，1.4 重连验证需兼容「同代际部分会话失效 → 全量重盖章」；W1 需 1.4 自造事务负载结构或回补一个视图类型。
4. **建议回补方案**（若后续 PR 需要）：新增 `_SessionRegistryView` 三元组对象作为 launcher 侧会话级代数锚点，由 `_MpiL3GroupRuntime` 持有；`_expire_mpi_group_session` 的失效与 `_invalidate_endpoint_registry()` 解耦，改为只作废涉事 group 的绑定，registry 换代仅由真正拓扑失效触发。

---

> **范围**：simpler 仓 L4 会话层；本文档为设计评估，不涉及代码改动。
