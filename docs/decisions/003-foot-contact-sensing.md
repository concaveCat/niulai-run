# Decision: Foot Contact Sensing

Status:
- Proposed

Problem:
SEARCH 何时恢复 SUPPORT 需要可靠、低延迟且适合实体结构的触地判定。

Options:
足端开关、力/压力传感、关节电流/扭矩推断或其他方案。

Current hypothesis:
先采用能最快安装和观察的方案进行单腿验证，不预设最终传感器。

Experiment:
比较候选方案在不同地面、速度和落脚角度下的误触发、漏触发和延迟。

Result:
已完成一针电气检查：在 ESP32-S3 GPIO39 使用内部上拉、C 接 GND、NC 接 GPIO39，GPIO39 CLI 实测 C-NC 开路为 `1`、短路为 `0`。当前四路驱动按 Operator 指定映射使用 GPIO39=FR、GPIO40=FL、GPIO41=RR、GPIO42=RL；按当前安装状态，FL 保持低电平触发，FR/RL/RR 使用高电平触发。该映射尚未由四路实体台架记录独立验证，机械触地方向和开关抖动时长仍待实验。

Decision:
暂不冻结。

Consequences:
会影响 hardware sensor adapter 和 SEARCH 参数；FSM 只接收布尔 contact capability。
