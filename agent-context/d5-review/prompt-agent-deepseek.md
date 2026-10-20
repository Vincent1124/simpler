# 给 agent-deepseek 的任务指令

请从个人 fork `https://github.com/Vincent1124/simpler.git` 拉取 `mdtest` 分支最新内容，并按以下要求进行独立评审。你在本目录中的角色名为 `agent-deepseek`，另一个环境的角色名为 `agent-gpt`。

## 拉取与阅读

先检查工作区，妥善保存已有修改，不要 reset 或覆盖文件。确认 `origin` 指向上述个人 fork，然后执行：

```bash
git fetch origin
git switch mdtest
git pull --ff-only origin mdtest
```

若本地没有 `mdtest`，使用 `git switch --track origin/mdtest`。若分支被其他 worktree 使用，使用该 worktree 或另建工作分支，不强占分支。

请遵守仓库 `AGENTS.md`、`CLAUDE.md` 及适用规则，按以下顺序阅读：

1. `agent-context/d5-review/README.md`
2. `agent-context/d5-review/shared-context.md`
3. 根目录 `D5-issue.md`
4. 根目录 `simpler-pr-1.1-design.md`
5. `agent-context/d5-review/agent-gpt-review.md`

## 评审任务

请独立分析 `D5-issue.md` 是否合理，并逐项回应 agent-gpt 的 A1—A6。A1—A6 是稳定的问题编号，不表示角色名称。不必迁就对方结论；请区分代码事实、需求依据、设计推论和建议。

特别注意：`mdtest` 的产品代码不包含 PR 2641 的实现。评估该 PR 时，请核对固定提交 `77b2f85618dc06ff7e8a32b42cbdbce3dfe4dafc`，不要将 `mdtest` 当前产品代码误认为 PR 实现。

将你的结论、证据、同意或反对理由，以及仍需用户决策的问题，写入 `agent-context/d5-review/agent-deepseek-review.md`。请记录实际审阅的文档提交、产品代码提交、PR 清单访问结果，以及未执行的验证。

本轮仅修改你负责的评审文件，不修改 `agent-gpt-review.md`、两份根目录文档或产品代码。保持分歧及证据可追溯，不将尚未确认的结论写成双方共识。

## 提交与同步

完成检查后，按仓库规范提交，并普通 push 到个人 fork 的 `mdtest`。只暂存你的评审文件：

```bash
git add agent-context/d5-review/agent-deepseek-review.md
git diff --cached --check
git diff --cached
git commit -m "Update: record independent D5 review findings"
git push origin HEAD:mdtest
```

遵守仓库 hooks 和提交规范。若远端已更新，先整合远端意见并检查冲突后再推送，不要 force push。最后报告提交 SHA，供 agent-gpt 所在环境拉取复核。
