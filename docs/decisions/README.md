# Decision Records

Decision Record 用来记录“为什么这样选”，而不是提前宣告最终架构。状态含义：

- **Proposed**：提出候选和当前假设，尚未验证。
- **Testing**：已有实验正在验证。
- **Accepted**：当前阶段采用；仍可被新证据 supersede。
- **Rejected**：当前阶段不采用。
- **Superseded**：被后续记录替代，并保留链接和原因。

提交新的重大方案变化时，复制 [`TEMPLATE.md`](TEMPLATE.md)，使用有意义的文件名，并在 Result 之后更新状态。不要删除历史记录。

当前未冻结事项总表见 [`OPEN_DECISIONS.md`](OPEN_DECISIONS.md)。

全部现有记录、状态和并行 `010` 文件名说明见 [`INDEX.md`](INDEX.md)。
