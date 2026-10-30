# Carrier 纸面检查

本目录只保留当前 Carrier v0.2 的可复现纸面检查。脚本通过**不等于** PCB DRC、实物验证或满载安全证明。

| 脚本 | 检查内容 |
| --- | --- |
| `verify_modular_carrier_interface.py` | 接口表、GPIO 冲突、8 路 PWM、4 路足端、MPU/ST 飞线策略、空闲 GPIO breakout、`/OE` 默认态。 |
| `verify_modular_carrier_power_protection.py` | 单 XT30 输入、10 A 外部保险契约、≥10 A 主路径等级、可选 servo bulk 契约。 |

运行：

```bash
python3 hardware/electronics/calculations/verify_modular_carrier_interface.py
python3 hardware/electronics/calculations/verify_modular_carrier_power_protection.py
```

两项检查也包含在 `sh tools/run_offline_checks.sh` 中。

已作废集成控制器的电池、SY8388、LM74800、ESP32 旧分配、音频/LED 和历史 front-end 计算已移至 [`../archive/legacy-integrated-controller/calculations/`](../archive/legacy-integrated-controller/calculations/)。

> 纸面检查只证明表格/约束的一致性，不证明电气裕量、信号完整性、装配正确性或实物安全。
