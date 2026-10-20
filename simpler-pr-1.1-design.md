# PR 1.1 设计：MPI 会话身份与既有 EndpointRegistry 的关联、失效边界

> 基线：simpler HEAD `653a24c4a`（2026-10-15 静态复核口径）。
> 前置问题（来自 PR 清单 `DET['1.1'].s`）：*「如果当前只允许固定成员、整组重启，独立的会话 epoch 还要解决什么场景。」* 本文先回答这个问题，再据此划定最小改动范围——**只做关联与失效边界，不新建注册表**。

---

## 1. 背景与目标

### 1.1 这份 PR 要做什么

把 **MPI 会话身份**（每个 `mpirun` 会话的 `session_id`、组内 `worker_id`）与已有的 **`EndpointRegistry` 身份体系**（`session_instance_id` + `registry_epoch`）**关联**起来，并**定清两者各自的失效边界**。**不是**再造第二份通用注册表。

当前 `EndpointRegistry` 已经带 session nonce 和 epoch，委派分配（`comm_region.py`）也在检查它们，拒绝过期 epoch 的两处逻辑都在。缺的是：**mpirun 会话换代时这个 epoch 不动**——`registry_epoch` 是单个 Worker 的拓扑代数，不是会话代数；MPI 帧头目前只有 `session_id`/`worker_id`/`sequence`，不携带 registry 身份。

### 1.2 为什么只做「关联与失效边界」

`DET['1.1'].s` 要求先回答：*「如果当前只允许固定成员、整组重启，独立的会话 epoch 还要解决什么场景。」* 本文第 2 章的结论是：**在固定成员 + 整组重启的当前约束下，独立会话 epoch 不解决任何现有代码路径能触达的场景**——因为它想防的「跨会话的旧资源被新会话误用」已经被更基础的机制拦住了（见 2.2）。因此不引入独立的会话 epoch，只把「会话身份」与「registry 身份」的**关联**和**失效边界**写清楚、补成可验证的约束。

### 1.3 层级定位

PR 清单给出的定位：**L4 会话层，在 mailbox 之上、建域事务之下**。

```text
L4 Worker（launcher）
├─ 拓扑快照 → EndpointRegistry（session_instance_id + registry_epoch）   ← registry 身份
├─ mailbox（rank-0 共享内存组信箱）                                      ← 会话启动/就绪
├─ MPI 帧头（session_id / worker_id / sequence）                         ← 会话身份（帧级）
└─ Global CommDomain 四阶段事务（PREPARE/IMPORT/COMMIT/ABORT）           ← 建域事务（1.4 范围）
```

---

## 2. 前置问题：独立会话 epoch 还要解决什么场景

### 2.1 现状：两类身份，两套代数

| 身份 | 携带者 | 生成 | 换代时机 | 作用域 |
| --- | --- | --- | --- | --- |
| **registry 身份**：`session_instance_id` + `registry_epoch` | `EndpointIdentity`、`EndpointRegistry`（`comm_endpoints.py`） | `session_instance_id` = `worker._owner_instance_id`（每 Worker 实体唯一 nonce，`buffer.py:123` `mint_owner_instance_id` = `os.urandom`）；`registry_epoch` 初始 0（`worker.py:5392`），仅 `_invalidate_endpoint_registry()`（`worker.py:7091-7094`）自增 | 单次 Worker 化身（startup→close） | 单个 Worker 的拓扑代数；两处拒绝点（`comm_region.py:426-429`、`:1328-1335`） |
| **MPI 会话身份**：`session_id` + `worker_id` | 远端线协议帧头（`remote_l3_protocol.py:121-127`）、MPI group manifest、`mpi_l3_session.py` 会话主循环 | `session_id` = `_new_remote_session_id()`（`worker.py:5961-5963`）= `uuid.uuid4().int & ((1<<63)-1)`；`worker_id` = 拓扑分配的 NEXT_LEVEL id（`worker.py:5515`） | mpirun 整组重启（`add_mpirun_worker_group` → `_activate_mpirun_worker_groups` → `_close_mpirun_group`） | 会话代数；帧级路由身份 |

### 2.2 关键事实：registry epoch 与 MPI 会话是两套独立代数

1. **`registry_epoch` 是单 Worker 的拓扑代数，不是会话代数**。它在 `_invalidate_endpoint_registry()`（`worker.py:7091`）自增，该函数只在 `close()`（`worker.py:13178`）调用。它度量的是「同一 Worker 实体的端点在本次化身内经历了几次拓扑失效」，与 mpirun 会话无关。
2. **MPI 会话换代时 registry epoch 不动**。`mpirun` 整组重启走 `_close_mpirun_group` → 重新 `_activate_mpirun_worker_groups`。如果 L4 launcher 是同一个进程且不 close，其 `_endpoint_registry_epoch` 不会因会话换代而变化。
3. **MPI 帧头不携带 registry 身份**。`FrameHeader(frame_type, session_id, worker_id, sequence, payload_bytes, flags)`（`remote_l3_protocol.py:121-127`），编码 `<IIQiQII`（`:374`）。`_rewrite_frame_identity`（`mpi_l3_session.py:305-319`）只重写 `session_id`/`worker_id`/`sequence`。
4. **会话身份在协议/事务层有独立校验**。`COMM_INIT` 校验 cluster_id/profile/node_rank/global_device_ranks 与 manifest 一致（`remote_l3_session.py:1043-1073`）；`_validate_manifest` 要求 `session_id` 非零（`remote_l3_worker.py:60`）。

### 2.3 回答：固定成员 + 整组重启场景下，独立会话 epoch 解决什么

**结论：什么都不解决，且现有代码路径触达不到它想防的场景。** 推理如下：

- **「跨会话误用」已经被更基础的机制拦住**：
  - 整组重启后，新的 `mpirun` 是一个全新的进程组（`_close_mpirun_group` 回收旧进程；`_activate_mpirun_worker_groups` 用新的 `session_id` 重新 `mpirun`）。每个 rank 的 `inner_worker` 是 `mpi_l3_session.py:418` 新建的 `Worker`，其 `_owner_instance_id` 是新的 `os.urandom` nonce（`buffer.py:123`）。因此**新会话的每个 registry 身份天然与旧会话不同**——不是靠「epoch 变了」区分，而是靠「nonce 变了」区分。
  - 会话级路由身份 `session_id` 也是新 `uuid`（`worker.py:5961`），帧头 `session_id` 不匹配即拒绝（`remote_l3_session.py:734-735`）。
- **固定成员**：成员表在 `MpiL3GroupSpec` 构造时冻结（`worker.py:1016` 等），`add_mpirun_worker_group` 在 `init()` 前必须完成（`worker.py:5504-5506`），拓扑在 `init()` 冻结。成员在会话代数内不变。
- **「独立会话 epoch 要防的」=「旧会话的 registry 引用被新会话沿用」**：这要求旧 registry 身份（旧 nonce/epoch 组合）能被新会话继续当有效身份使用。但新会话的 nonce 必然不同，`_validate_registry_matches_worker`（`comm_region.py:1328-1335`）比较 `worker._owner_instance_id == registry.session_instance_id` 且 `_endpoint_registry_epoch == registry.registry_epoch`——旧 nonce 不匹配新 worker，直接拒绝。**跨会话沿用旧身份，在现有代码里根本过不了第一道 nonce 校验。**

因此：**在当前约束（固定成员、整组重启）下，不需要独立的会话 epoch。** 独立会话 epoch 只有在下述场景才有意义（全部不在当前约束内，本文不实现）：

1. **成员可变**（会话中增删成员）——那时需要「成员集合代数」来区分新旧成员表。当前成员冻结，无此需要。
2. **部分存活/热迁移**（跨会话保留部分 worker，如 1.3 的物理独占、1.8 的跨会话存续）——那时才需要区分「同一 worker 的两个会话化身」并拒绝旧化身的资源请求。
3. **旧会话资源跨新会话被显式引用**（如 1.4 的「跨代 COMMIT 没人拦」）——但那属于建域事务层（`GlobalDomainCommand` 只带 `generation`），不是会话层。

> **对 1.1 的结论**：只做「关联 + 失效边界」，不引入会话 epoch。关联是**记录事实**（会话身份 ↔ registry 身份的对偶），不是**新增代数**（不把 session_id 塞进 registry epoch 判定，也不把 registry epoch 塞进帧头）。

---

## 3. 现状事实（代码锚点）

### 3.1 EndpointRegistry 身份体系

| 事实 | 位置 |
| --- | --- |
| `EndpointIdentity(session_instance_id, registry_epoch, endpoint_id)` | `comm_endpoints.py:199-202` |
| `EndpointRegistry(session_instance_id, registry_epoch, ...)` | `comm_endpoints.py:246-259` |
| `from_snapshot` 用 `snapshot.session_instance_id` + 传入 `registry_epoch` 铸造每个 identity | `comm_endpoints.py:273-335`（`:301-302`） |
| `_EndpointTopologySnapshot(session_instance_id, entries)` | `comm_endpoints.py:186-190` |
| `worker._endpoint_registry_epoch: int = 0`（初始化） | `worker.py:5392` |
| `_invalidate_endpoint_registry()`：`epoch += 1`、registry 置 None | `worker.py:7091-7094` |
| `_get_endpoint_registry()`：按当前 epoch 惰性重建 | `worker.py:7220-7228` |
| `_endpoint_topology_snapshot()`：`session_instance_id=self._owner_instance_id` | `worker.py:7112-7123`（`:7121`） |
| `_owner_instance_id = mint_owner_instance_id()` = `os.urandom` | `worker.py:5414`、`buffer.py:123-130` |
| `close()` 中调用 `_invalidate_endpoint_registry()` | `worker.py:13178` |

### 3.2 两处现有拒绝点（registry 身份的一致性校验）

| 拒绝点 | 位置 | 校验内容 |
| --- | --- | --- |
| ① `_delegated_allocate_dispatch` | `comm_region.py:414-441`（核心 `:418-430`） | 同一 Worker 化身内 `_delegated_session_instance_id` 与 registry 的 `session_instance_id` 必须一致（`:419-425`）；`registry.registry_epoch` 必须等于 `expected_registry_epoch`（`:426-430`），否则 `REGISTRY_MISMATCH` |
| ② `_validate_registry_matches_worker` | `comm_region.py:1328-1335` | `worker._owner_instance_id == registry.session_instance_id` 且 `worker._endpoint_registry_epoch == registry.registry_epoch`，否则 `REGISTRY_MISMATCH` |
| 调用链 | `comm_region.py:1030-1033`（`_delegated_allocate_dispatch(registry=ctx.registry, expected_registry_epoch=ctx.registry.registry_epoch)`） | 见 ① |

> 注意：① 的 epoch 校验是**自洽**校验（`registry.registry_epoch` 与 `expected_registry_epoch` 相同，二者都来自同一 `ctx.registry`），它实际防的是「同一 Worker 化身内两次注册表换代后，旧 `RegionInstance` 被用于新的委派分配」。② 才是真正把 registry 钉在**当前 worker 化身**上的校验。

### 3.3 MPI 会话身份体系

| 事实 | 位置 |
| --- | --- |
| `FrameHeader(frame_type, session_id, worker_id, sequence, payload_bytes, flags)` | `remote_l3_protocol.py:120-127` |
| 帧头编码 `<IIQiQII`（version, raw_type, session_id, worker_id, sequence, payload_bytes, flags） | `remote_l3_protocol.py:366-384`（`:374`） |
| `_new_remote_session_id()` = `uuid.uuid4().int & ((1<<63)-1)` | `worker.py:5960-5963` |
| `add_mpirun_worker_group`：每 rank 一个 `session_id`，`group_id = uuid4().hex` | `worker.py:5497-5537`（`:5512, :5529`） |
| `_build_mpi_rank_manifest`：manifest 带 `session_id`/`worker_id`/`cluster_id`/`global_device_ranks`/`mpi_*` | `worker.py:5877-5906` |
| `_activate_mpirun_worker_groups`：写 group_manifest → `mpirun -m simpler.mpi_l3_session` → 等 mailbox READY | `worker.py:6063-6145`（`:6119-6128`） |
| `_rewrite_frame_identity`：按 manifest 重写帧头 `session_id`/`worker_id`/`sequence` | `mpi_l3_session.py:305-319` |
| `_validate_manifest`：`session_id` 非零、`worker_id` 非负 | `remote_l3_worker.py:55-63` |
| 会话主循环校验：`header.session_id != session_id or header.worker_id != worker_id` → 拒绝 | `remote_l3_session.py:734-735` |
| `COMM_INIT` 校验：cluster_id/profile/node_rank/global_device_ranks 与 manifest 一致 | `remote_l3_session.py:1043-1073` |
| mailbox 头部预留区：`MAILBOX_HEADER_BYTES = 256`、`_OFF_RESERVED = 80`，创建方清零、attach 拒绝非零 | `mpi_group_mailbox.py:31, 54-57, 461-462` |

### 3.4 会话与 registry 目前如何「碰面」（关联现状）

- **拓扑快照把 MPI rank 当成普通远端端点**：`_append_endpoint_topology`（`worker.py:7160-7167`）为每个 MPI rank 追加 `_EndpointTopologyEntry(mpi_path, HOST_CPU, mpi_node_identity)`（无 owner nonce），并追加其设备端点。这些 entry 进入 `_EndpointTopologySnapshot`，从而进入 registry——**但 registry 只知道「有一个 MPI rank 端点」，不知道它属于哪个会话、哪个 `session_id`**。
- **manifest 是会话身份的唯一传播通道**：`_build_mpi_rank_manifest`（`worker.py:5877`）把 `session_id`/`worker_id`/`cluster_id`/`mpi_group_id` 传给 rank；`mpi_l3_session.py` 用 manifest 构造 inner_worker 并驱动 `_run_command_loop`。**manifest 里没有 registry 身份字段**（没有 `session_instance_id`/`registry_epoch`）。
- **会话主循环不触碰 registry**：`_run_command_loop`（`remote_l3_session.py:681+`）只做命令分发；inner_worker 的 registry 在 `_get_endpoint_registry()` 惰性构建（`worker.py:7220`）。

---

## 4. 问题定义

### 4.1 核心矛盾

`EndpointRegistry` 的 `registry_epoch` 是 **单个 Worker 化身**的拓扑代数，而 MPI `session_id` 是**会话代数**。两者目前：

1. **不关联**：`session_id` 不出现在 `EndpointIdentity`/`EndpointRegistry` 里；`session_instance_id`/`registry_epoch` 不出现在 MPI 帧头、manifest、会话主循环里。
2. **换代不同步**：mpirun 整组重启（会话换代）时，若 L4 launcher 进程不 close，`_endpoint_registry_epoch` 不变；反之，同一会话内若 launcher `close()`（registry 换代），`session_id` 不变。

### 4.2 现状下的失效边界（已存在但未被命名/验证）

虽然两套身份不关联，但**现有代码的失效边界是完整且安全的**：

- **会话代数边界**：`session_id`（uuid，`worker.py:5961`）→ manifest → 帧头。换代 → 新 uuid → 帧头 `session_id` 不匹配即拒（`remote_l3_session.py:734`）。
- **registry 代数边界**：`_owner_instance_id`（urandom，`worker.py:5414`）+ `registry_epoch`（`worker.py:5392/7094`）→ `EndpointIdentity` → 两处拒绝点（`comm_region.py:426,1331`）。换代 → 新 nonce（或 epoch+1）→ `REGISTRY_MISMATCH`。
- **建域代数边界**：`cluster_id`（`worker.py:5368`）+ `domain_id`+`generation`（`global_comm_domain.py`）→ `COMM_INIT` 校验（`remote_l3_session.py:1046`）。

**缺口**：这三条边界**互不知道对方存在**，没有任何代码在「会话↔registry」或「会话↔建域」交界处做交叉校验或显式声明失效。跨会话/跨代复用是否发生，全凭上述独立机制各自兜底，缺少一处显式的「会话身份 ↔ registry 身份」对偶记录。

### 4.3 需要补的（本 PR 范围）

1. **关联**：把「MPI 会话身份」与「registry 身份」的**对偶关系**显式记录为可检查的不变量——即：*「一次 mpirun 会话 = 一个 registry 代数视图」* 与 *「一个 registry 代数视图属于且仅属于一次会话」*。
2. **失效边界**：在会话换代 / registry 换代的交界处，把现有「靠各自 nonce 兜底」升级为「显式声明并拒绝」，使失效成为可诊断的失败（`REGISTRY_MISMATCH`/新的会话不匹配错误），而不是隐式的后续错误。

---

## 5. 设计决策

### 5.1 决策总览

| # | 决策 | 理由 |
| --- | --- | --- |
| D1 | **不引入独立的会话 epoch** | 第 2 章结论：固定成员 + 整组重启下无场景可解；引入只会扩成新注册表（清单明确禁止） |
| D2 | **会话身份与 registry 身份做单向关联（会话 → registry 视图）**，方向是「一次会话对应一个 registry 代数视图」，记录在 launcher（L4）侧 | 关联是记录事实，不是新增代数；launcher 是唯一同时持有两者（`session_id` + `_owner_instance_id`/`_endpoint_registry_epoch`）的进程 |
| D3 | **帧头不新增字段**，`session_id`/`worker_id`/`sequence` 保持现状 | 帧头是线协议，`PROTOCOL_VERSION=4` 严格相等（`remote_l3_protocol.py:32,393`）；会话身份已足以路由/校验帧，加 epoch 属于 1.11（版本协商）的负担且无场景收益 |
| D4 | **manifest 增加只读的 registry 视图描述字段**（会话代数锚点），供 rank 侧 inner_worker 校验自己属于哪个 registry 视图 | manifest 已是会话身份唯一传播通道（3.4）；加只读字段不破坏现有字段；rank 侧可据此做「我的 registry 代数视图」自检 |
| D5 | **registry 视图代数 = 一次会话的「代数快照」**：`session_id`（会话）+ `session_instance_id` + `registry_epoch`（registry）三元组；换代任一元即换视图 | 把 4.2 的三条边界之一（会话↔registry）显式化为一个可比较的元组，为 1.4（建域事务关联）预留代数锚点 |
| D6 | **失效判定放在 L4 会话层（launcher）**，不是放在远端 rank 或建域事务层 | 清单定位：L4 会话层、mailbox 之上、建域事务之下；launcher 是会话生命周期（`add_mpirun_worker_group`→`_activate`→`_close`）的唯一持有者 |
| D7 | **会话换代 / registry 换代在 launcher 侧互相使对方失效**：会话换代时，把该会话对应的 registry 视图标记为「已随会话退役」；registry 换代（`_invalidate_endpoint_registry`）时，该会话视图自动失效 | 显式化 4.2 的边界，使跨代误用变成可诊断失败 |

### 5.2 核心模型：会话 ↔ registry 视图 对偶

```text
MpiL3GroupRuntime（会话）            EndpointRegistry（registry 视图）
  session_id (uuid)   ──┐            session_instance_id (urandom)
  group_id (uuid4hex)  ├─ D5 视图 ─→  registry_epoch (startup 代数)
  worker_ids           │             records / owner_bindings / ...
  mpi_* (manifest)     ┘
```

- **视图** = `(session_id, session_instance_id, registry_epoch)`。
- 一次会话启动时，launcher 记录该会话的视图（`session_id` 已知，`session_instance_id`/`registry_epoch` 为启动时的 launcher 值）。
- 会话换代（`_close_mpirun_group` 后重新 `_activate`）→ 新 `session_id` → 新视图。
- registry 换代（`_invalidate_endpoint_registry`）→ `registry_epoch+1`（或新 nonce）→ 同一 `session_id` 下视图换代。

### 5.3 为什么不在帧头/协议层做关联（D3 的展开）

- **协议成本**：帧头格式 `<IIQiQII` 固定，加字段要 bump `PROTOCOL_VERSION`（4→5），触发 1.11（版本协商）的全部负担；当前 `version != PROTOCOL_VERSION` 直接抛错（`remote_l3_protocol.py:393`），滚动升级两端必然短暂不一致。
- **无场景收益**：帧级路由/校验只需要 `session_id`/`worker_id`/`sequence`。registry epoch 是 launcher 侧代数，rank 侧 inner_worker 自己也有 `_owner_instance_id`/`_endpoint_registry_epoch`，不需要从帧头得知 launcher 的。
- **与 1.4 的分工**：建域事务需要的是「域 generation 之外补 registry epoch 关联」（1.4 范围，`GlobalDomainCommand` 只带 `generation`），那在事务命令负载里加字段，不在帧头。

### 5.4 为什么 manifest 加只读描述（D4 的展开）

manifest 已是 `session_id`/`worker_id`/`cluster_id` 等会话身份的传播通道（`worker.py:5877-5906`），且远端 `_validate_manifest` 已校验必填字段（`remote_l3_worker.py:55-63`）。增加**只读的 registry 视图描述字段**（如 `registry_view: {session_instance_id, registry_epoch}`），成本最低、不破线协议，并让 rank 侧 inner_worker 能自检「我的 registry 视图 = launcher 声明的那一个」。

> 注意：rank 侧 inner_worker 的 `_owner_instance_id` 是**它自己**的 nonce（`mpi_l3_session.py:418` 新建的 `Worker`，`worker.py:5414`），不是 launcher 的。所以 D4 的字段描述的是「launcher 的 registry 视图」（供校验/诊断），不是让 rank 侧伪装成 launcher 身份。rank 侧自己的视图由它自己的 nonce 决定。

---

## 6. 改动清单（按仓与文件）

> **范围结论**：全部改动集中在 **simpler 仓**。pypto / pypto-lib / pto-isa / PTOAS **不涉及**（本 PR 是会话层语义，无 kernel 层触点；1.5 才处理 pypto 交接）。改动按「必须 / 建议 / 可选」分三级，保持最小改动面。

### 6.1 改动地图

| 文件（simpler 仓） | 层级 | 改动性质 | 对应决策 |
| --- | --- | --- | --- |
| `python/simpler/worker.py` | L4 launcher | **必须**：会话视图记录 + 换代失效 + 校验入口 | D2/D5/D6/D7 |
| `python/simpler/comm_endpoints.py` | 身份模型 | **可选**：`EndpointRegistry` 只读视图元数据（不强制） | D5 |
| `python/simpler/comm_region.py` | 委派分配 | **可选**：两处拒绝点后追加会话视图有效性检查 | D6 |
| `python/simpler/mpi_l3_session.py` | L4 rank | **建议**：manifest 消费只读自检（诊断日志） | D4 |
| `python/simpler/remote_l3_protocol.py` | 线协议 | **零改动**（帧头保持 `<IIQiQII`） | D3 |
| `python/simpler/remote_l3_session.py` / `remote_l3_worker.py` | 会话循环 | **零改动**（帧头/COMM_INIT 校验已够） | D3 |

### 6.2 A. worker.py —— 会话视图记录与失效（必须）

**改动原因**：launcher（L4）是唯一同时持有 `session_id` 与 `_owner_instance_id`/`_endpoint_registry_epoch` 的进程（5.1 D2），会话 ↔ registry 的关联事实必须记录在这里，否则两套代数永远各走各的（4.1）。

**新增类型与状态**：

```text
@dataclass(frozen=True)
class _SessionRegistryView:          # D5 三元组
    session_id: int                  # 会话代数锚点（worker.py:5961 生成的 uuid）
    session_instance_id: bytes       # registry nonce（= launcher._owner_instance_id）
    registry_epoch: int              # registry 拓扑代数（launcher._endpoint_registry_epoch）
```

- `self._session_registry_views: dict[group_id, _SessionRegistryView]`（按 `_MpiL3RankRuntime.group_id` 索引，group_id 是 `uuid4().hex`，worker.py:5512）。

**改动点（对应 5.2 的模型）**：

| 改动点 | 位置（现状） | 改法 | 原因 |
| --- | --- | --- | --- |
| ① 会话启动记录视图 | `add_mpirun_worker_group`（worker.py:5497-5537）建 `_MpiL3RankRuntime` 时 | 每 rank 记录 `(group_id → 当前 launcher 的 _owner_instance_id + _endpoint_registry_epoch)` 快照 | 会话启动时刻的 registry 代数视图是「该会话的代数锚点」 |
| ② 会话换代失效 | `_close_mpirun_group`（worker.py:5970-6033） | 关闭会话时，将其 `_session_registry_views[group_id]` 标记/移除，并记录「已随会话退役」；重新 `_activate` 用新 `session_id` 记新视图 | D7：旧会话的 registry 视图不得被新会话沿用 |
| ③ registry 换代失效 | `_invalidate_endpoint_registry`（worker.py:7091-7094） | epoch+1 后，遍历所有 `_session_registry_views`，凡 `registry_epoch != 新 epoch` 且 `session_instance_id == 当前 nonce` 的视图标记失效 | D7：registry 换代时该会话视图自动失效 |
| ④ 校验入口 | 新增 `_validate_session_registry_view(group_id, ...)` | 查询 `_session_registry_views[group_id]`，与当前 launcher 代数比对，不一致抛可诊断错误（复用 `REGISTRY_MISMATCH` 语义） | D6：失效判定放 launcher 侧，成为可诊断失败而非隐式后续错误 |

> **不动的部分**：`_endpoint_registry_epoch` 的初始值/自增逻辑（worker.py:5392/7091）、`_owner_instance_id`（worker.py:5414）、拓扑快照（worker.py:7112-7123）、`_get_endpoint_registry`（worker.py:7220-7228）——都保持原样，**不引入第二个注册表、不新增代数**。

### 6.3 B. comm_endpoints.py —— 身份模型最小触点（可选）

**改动原因**：让 `EndpointRegistry` 可选携带只读的会话视图元数据，便于诊断（「这个 registry 视图属于哪个会话」）。**不新增强制字段**——`EndpointIdentity`/`EndpointRegistry` 构造器签名不变，避免破坏现有调用（`from_snapshot`、`_get_endpoint_registry` 等）。

- 可选：给 `EndpointRegistry` 加一个只读 `session_view: _SessionRegistryView | None = None` 属性（默认 None），由 launcher 在记录会话时回填，仅用于诊断/日志，**不参与**任何身份判定。
- **边界**：`session_instance_id`/`registry_epoch` 仍是唯一身份判定依据（comm_endpoints.py:199-202、246-259）；session_view 是「记录事实」，不是「新增判定代数」——严格对应 2.3 的结论。

### 6.4 C. comm_region.py —— 拒绝点增强（可选）

**改动原因**：两处现有 `REGISTRY_MISMATCH` 拒绝点（comm_region.py:426-430、1328-1335）已经钉住「当前 worker 化身」，追加会话视图检查只补「显式诊断」一层，不改变判定结果（2.2 已证明 nonce 换代机制已拦）。

- 可选：在拒绝点①/② 命中后，若 launcher 侧存在 `_session_registry_views` 且视图已标记失效，把错误信息升级为「会话已换代 / registry 已换代」的可读诊断（复用 `REGISTRY_MISMATCH` 错误码，不新增协议错误码）。
- **不做**：不在 `comm_region.py` 引入会话层状态——它是委派分配层，不是会话生命周期持有者（D6 边界）。

### 6.5 D. mpi_l3_session.py —— manifest 只读自检（建议）

**改动原因**：manifest 是会话身份唯一传播通道（3.4），加只读 `registry_view` 字段让 rank 侧 inner_worker 能自检「launcher 声明的 registry 视图」与自己的代数是否一致，作为诊断日志。

- `_build_mpi_rank_manifest`（worker.py:5877-5906）增加只读字段 `registry_view: {session_instance_id, registry_epoch}`（描述 launcher 的视图，非 rank 身份）。
- `mpi_l3_session.py` 消费该字段：**只记录诊断日志**，不据此拒绝——rank 侧 inner_worker 的 `_owner_instance_id` 是它自己的 nonce（5.4 注意），不能用 launcher 的视图做身份校验。
- **零改动**：`_rewrite_frame_identity`（mpi_l3_session.py:305-319）、帧头（remote_l3_protocol.py:120-127）不动（D3）。

### 6.6 E. 不改动的文件（显式声明）

| 文件 | 不改原因 |
| --- | --- |
| `remote_l3_protocol.py` | D3：帧头 `<IIQiQII`、`PROTOCOL_VERSION=4` 严格相等，改动触发 1.11 版本协商连锁 |
| `remote_l3_session.py` | 帧头校验（:734-735）与 COMM_INIT 校验（:1043-1073）已覆盖会话身份；不引入会话层状态到远端主循环 |
| `remote_l3_worker.py` | `_validate_manifest`（:55-63）只校验必填字段；新增字段是只读可选，不参与该校验 |
| `global_comm_domain.py` | 建域事务只带 `generation`（:204-215）；补 registry epoch 关联属于 **1.4** 范围，本 PR 只留锚点（D5） |
| `mpi_group_mailbox.py` | 预留区（:31,54-57）是 1.2（租约）范围，本 PR 不动 |
| pypto / pypto-lib / pto-isa / PTOAS | 无会话层触点，不涉及 |

---

## 7. 测试与验收

### 7.1 单元测试（UT，sim 平台）

> 参考现有范式：`tests/ut/py/test_global_comm_domain.py:669` 的故障注入 / 代数校验写法。sim 平台不需要真机，覆盖会话层语义。

| # | 用例 | 覆盖改动点 | 断言 |
| --- | --- | --- | --- |
| T1 | 会话启动记录视图 | 6.2① | `add_mpirun_worker_group` 后，`_session_registry_views[group_id]` 存在，且 `(session_id, session_instance_id, registry_epoch)` 等于启动时 launcher 值 |
| T2 | 会话换代失效 | 6.2② | `_close_mpirun_group` 后视图被标记/移除；重新 `_activate` 用新 `session_id`，旧视图不可再用 |
| T3 | registry 换代失效 | 6.2③ | `_invalidate_endpoint_registry` 后，持有旧 `registry_epoch` 的会话视图被标记失效 |
| T4 | manifest 携带 registry_view | 6.5 | `_build_mpi_rank_manifest` 输出含只读 `registry_view` 字段，且值 = launcher 当前视图 |
| T5 | 帧头零改动 | D3 | `remote_l3_protocol.py` 帧头编解码往返不变（`<IIQiQII`、`PROTOCOL_VERSION=4`） |
| T6 | 拒绝点回归 | 6.4 | 现有 `REGISTRY_MISMATCH` 两处（comm_region.py:426、1331）行为不变；新增会话视图失效时给出可读诊断 |
| T7 | 跨会话复用拒绝（关键） | D7 | 构造「会话 A 视图 + 会话 B 的 registry 身份」→ launcher 侧校验拒绝，报会话不匹配 |

### 7.2 系统测试（ST）

- **sim 平台**：跑通 `mpirun` 整组启动/关闭 → 启动时视图记录、关闭后视图失效；会话换代（关→开）后，旧会话的委派分配请求被拒。
- **真机（master37，A3 卡，可选）**：`--ep 2` 跑通 `decode_moe.py` 冒烟，确认改动不影响现有 MoE 推理路径（本 PR 不动 kernel 层，预期无性能影响）。

### 7.3 验收标准

1. 现有测试全绿（sim 平台 UT + ST），无行为回退。
2. 会话换代 / registry 换代 → 显式可诊断失效（`REGISTRY_MISMATCH` 语义复用），而非隐式后续错误。
3. **无新增注册表、无新增代数**：`_endpoint_registry_epoch` 初始/自增逻辑、`_owner_instance_id`、拓扑快照、帧头全部保持原样（对照 6.2「不动的部分」与 6.6 不改文件清单）。
4. manifest 新增字段为只读可选，远端 `_validate_manifest` 不因缺该字段拒绝旧会话。

---

## 8. 风险与边界

| # | 风险 | 等级 | 缓解 |
| --- | --- | --- | --- |
| R1 | **过度设计扩成新注册表**：把「视图表」做成通用注册表，违背清单「不新建注册表」红线 | 高 | 视图表只按 `group_id` 索引、只存 D5 三元组、只做失效标记，无增删查以外的能力；6.2 明确「不动的部分」 |
| R2 | **与 1.2（租约）/ 1.4（建域关联）职责重叠**：把 mail 预留区或建域事务的活提前做掉 | 高 | 6.6 显式声明 `mpi_group_mailbox.py`、`global_comm_domain.py` 不改；1.4 只补 registry epoch 关联 = 本 PR D5 锚点，交接在文档层面 |
| R3 | **rank 侧身份误解**：rank 侧 inner_worker 误用 launcher 的 registry_view 做身份判定 | 中 | 5.4 注意 + 6.5 明确只读自检、仅诊断日志、不据此拒绝 |
| R4 | **帧头改动引发 1.11 连锁**：若后续 PR 想动帧头，会触发版本协商负担 | 低 | D3 把帧头钉死为零改动；如需加字段走 manifest/事务负载，不进帧头 |
| R5 | **本地仓只读无法验证编译**：Windows 本地只读阅读，simpler 运行环境在 Linux master37 | 中 | 7.3 验收标准 + 交付后由实现阶段在 master37 跑 sim UT；本 PR 纯 Python 语义改动，编译风险低 |
| R6 | **会话换代边界漏判**：`_close_mpirun_group` 之外的异常路径（进程崩溃）未走会话关闭 | 低 | 会话生命周期持有者（launcher）若崩溃，整组重启必然新 nonce（2.2），兜底仍安全；视图失效是显式化而非新依赖 |

---

## 9. 与其他 PR 的关系

### 9.1 批次定位

清单中 1.1 属第一批（DS-1，状态 now，无前置）。它把「会话 ↔ registry」边界钉住，是 1.2/1.3/1.4 的前置语义基础。

| PR | 依赖 1.1 | 与本 PR 的关系 |
| --- | --- | --- |
| **1.2**（mailbox 租约） | 是 | 1.2 用 mailbox 预留区（mpi_group_mailbox.py:31,54-57）做会话生命周期信号；1.1 的视图表提供「会话身份 → registry 视图」索引，1.2 的租约校验可引用该视图判定会话是否存活 |
| **1.3**（物理独占 / 跨会话存续） | 是 | 1.3 的「部分存活 / 热迁移」正是 2.3 列出的「独立会话 epoch 有意义」的场景之一；1.1 的 D5 三元组是 1.3 区分「同一 worker 两个会话化身」的代数基础 |
| **1.4**（建域事务补 registry epoch 关联） | 是 | `GlobalDomainCommand` 只带 `generation`（global_comm_domain.py:204-215）；1.4 补 registry epoch 关联 = 用本 PR D5 的锚点扩展事务负载，不在帧头 |
| **1.5**（pypto 交接） | 否 | 会话层语义与 kernel 层无关；1.5 单独处理跨仓交接 |
| **1.10**（清单「在飞建议先做」） | 否 | 建议优先，与本 PR 无冲突 |

### 9.2 明确不做（本 PR 边界）

- **不做 1.2**（mailbox 租约）、**不做 1.8**（跨会话存续）、**不做 1.5**（pypto 交接）——都在清单中有独立条目，本 PR 只留语义锚点。
- **不引入独立会话 epoch**（D1，2.3 结论）——这是对清单 DET['1.1'].s 的直接回应。

---

## 10. 结论摘要

1. **回答前置问题**（清单 DET['1.1'].s）：固定成员 + 整组重启下，独立会话 epoch **不解决任何现有代码路径能触达的场景**——跨会话误用已被 nonce 换代机制拦住（2.2/2.3）。独立会话 epoch 只在成员可变 / 部分存活 / 热迁移 / 跨会话显式引用旧资源时有意义（均不在当前约束内）。
2. **因此只做「关联 + 失效边界」**，不引入会话 epoch、不新建注册表（D1）。
3. **关联**：launcher（L4）记录「会话 ↔ registry 视图」对偶，视图 = `(session_id, session_instance_id, registry_epoch)` 三元组（D2/D5），记录在 `worker.py`（6.2）。
4. **失效边界**：会话换代 / registry 换代在 launcher 侧互相使对方失效，复用 `REGISTRY_MISMATCH` 语义成为可诊断失败（D6/D7）。
5. **协议零改动**：帧头 `<IIQiQII`、`PROTOCOL_VERSION=4` 不变（D3）；manifest 只加只读 `registry_view` 描述（D4）。
6. **改动集中在 simpler 仓**：`worker.py`（必须）+ `mpi_l3_session.py`（建议，只读自检）+ `comm_endpoints.py`/`comm_region.py`（可选，诊断增强）；pypto 等四个仓不涉及（第 6 章）。
7. **为 1.4 预留锚点**：D5 三元组是建域事务补 registry epoch 关联的代数基础（第 9 章）。
8. **验收**：sim UT（T1-T7）+ ST + 现有测试全绿 + 无新增注册表/代数（第 7 章）。

---

> **基线**：simpler `653a24c4a`（2026-10-15 静态复核口径）。
> **范围**：simpler 仓 L4 会话层，设计完成，待实现阶段在 master37 跑 sim UT 验证。
