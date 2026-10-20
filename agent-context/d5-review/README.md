# D5 评审上下文交换目录

本目录用于两个不同环境中的 agent 交换 D5 评审依据和意见，最终目的是更正代码缺陷与不合理之处。同步分支为个人 fork 的 `mdtest`；当前在这里完善修复依据与验收建议，不改写 `mpi-session-registry` 的历史。

## 阅读顺序

1. 阅读仓库根目录的 `AGENTS.md`、`CLAUDE.md` 及适用规则。
2. 阅读 [共同背景](shared-context.md)。
3. 阅读根目录的 [D5-issue](../../D5-issue.md) 和 [设计文档](../../simpler-pr-1.1-design.md)。
4. 阅读 [agent-gpt 评审](agent-gpt-review.md) 与 [agent-deepseek 评审](agent-deepseek-review.md)。
5. 核对 [用户范围决策](user-decison.md) 的状态；待确认项不能当作已批准需求。

给另一个环境的完整任务指令见 [agent-deepseek prompt](prompt-agent-deepseek.md)，可直接复制发送。

## 文件职责

| 文件 | 用途 | 修改约定 |
| --- | --- | --- |
| `shared-context.md` | 基线、范围、审阅问题和同步约定 | 修改时说明依据，不将争议结论写成事实 |
| `agent-gpt-review.md` | 当前 Codex 环境的判断和证据 | agent-gpt 维护 |
| `agent-deepseek-review.md` | 另一个环境的独立判断和逐项回应 | agent-deepseek 维护 |
| `prompt-agent-deepseek.md` | 可直接发送给 agent-deepseek 的任务指令 | 任务范围变化时同步维护 |
| `user-decison.md` | 仅承载需要用户定义的功能范围与边界 | agent 记录选项和影响，用户决定；不得代填决策 |

agent-gpt / agent-deepseek 只表示记录归属，不表示判断优先级。Git 历史保留各次意见，不应覆盖另一方文件来消除分歧。需要引用另一方意见时，注明文件和条目编号。

## 同步方法

工作区有未提交内容时先妥善保存；不要直接 reset、清理或覆盖文件。确认当前仓库的 `origin` 是目标个人 fork 后执行：

```bash
git fetch origin
git switch mdtest
git pull --ff-only origin mdtest
```

若尚无本地 `mdtest`，使用 `git switch --track origin/mdtest`。若该分支被其他 worktree 使用，使用该 worktree 或另建工作分支，不强占分支。

每次回应需要写明审阅的文档提交、产品代码提交以及实际检查方式。只暂存本次负责的文件：

```bash
git add agent-context/d5-review/agent-deepseek-review.md
git diff --cached --check
git diff --cached
git commit -m "Update: record independent D5 review findings"
git push origin HEAD:mdtest
```

提交应遵循仓库规则和 hooks。若 push 因远端前进被拒绝，重新 fetch，合并远端更新并检查冲突后再推送；不要 force push，也不要在存在未提交修改时直接合并。`pull --ff-only` 失败意味着需要处理分叉，并不意味着可以重置远端意见。

## 回应要求

- 将代码事实、需求依据、推论和建议分别表达。
- 对每个争议点给出同意、部分同意或不同意及原因。
- 使用固定提交的代码路径、符号和行号，避免只引用本地 HEAD。
- 未访问到的页面、未执行的测试和假设应明确标注。
- 若提出修改方案，解释失效传播、旧快照拒绝和事务清理的安全性。
- 目标是修正代码问题：每个问题应给出触发条件、影响、修复方向和验证方法；不要只停留在措辞争论。
- 用户范围选择仅放入 `user-decison.md`；代码事实、技术方案和测试方法由 agent 继续调查，不要求用户代做技术判断。
- 本轮更新评审与决策材料，产品代码实现和功能分支推送不属于本次文档修改。
