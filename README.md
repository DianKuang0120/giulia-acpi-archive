# giulia (SM8650) ACPI 存档

本目录保存 OnePlus Ace 5 / 13R（SM8650，代号 giulia）Windows on ARM 的 ACPI 调试成果。

## 目录结构

```
tables/           最终修正后的 ACPI 表（8 核）
  APIC.aml        MADT：8×GICC（80 字节）、独立 GICR、GICD、ITS
  DSDT.aml        DSDT：8 个 CPU（353 字节）
  FACP.aml        FADT：HW_REDUCED + LOW_POWER_S0_IDLE（原值）
  GTDT.aml        GTDT：NEL2=26、Always On
dsdt/             各种 DSDT 源文件
  DSDT.asl        8 核基线（最常用）
  DSDT_single.asl 单核诊断版
  DSDT_2cpu.asl   2 核诊断版
  DSDT_full_ufs_pcie.asl  加了 UFS/PCIe 的完整版（未采用）
tools/            二进制表修改工具（C 源码）
```

## 从 SM8635 继承的表里修掉的 7 处错误

原始 giulia 表是照搬 SM8635（Palawan）的，存在以下问题，均已修复：

1. **GICC Length 字段 = 52，实际间距 = 82** —— Windows 按 Length 跳转，
   从第 2 个 CPU 起解析错位。修法：`apictool fixlen`（改为 80 字节标准结构）。
2. **8 个核共用同一个 GICR 地址 `0x17180000`** —— 核间 redistributor 冲突。
   修法：`apictool gicr 0x17180000 0x40000`。
3. **多余的 0x0E GICR 区域结构** 覆盖逐核地址。修法：`apictool strip 0x0E`。
4. **GICC 长度 82 字节**（非标准，含 TRBE）。修法：`apictool shrink`（80 字节）。
5. **ITS 被写成 MSI Frame（0x0D）**，实际是 GIC ITS（0x0F）。修法：`apictool its`。
6. **GTDT NEL2 = 28**（不存在，SM8650 是 26）。修法：`gtdttool fix`。
7. **定时器缺 Always On 标志**。修法：`gtdttool fix`。

## 关键结论

- **多核（8 核）启动问题的根因不是 ACPI，而是 KEP 补丁**：
  `Silicon/Qualcomm/LanaiPkg/LanaiPkg.dsc.inc` 中
  `HAS_ACTLR_EL1_UNIMPLEMENTED_ERRATA` 应为 **0**（从 8635 继承来的值是 1）。
  该补丁把所有 `mrs actlr_el1` 改成 `mov x,#0`，而 ACTLR_EL1 bit6 是 SMP 位，
  导致次级核不启用 SMP，转半圈死机。

- **单核能启动、多核死机**是排查该问题时的关键现象（单核不需要 SMP 位）。

- 15 秒后桌面黑屏（需强制复位）是**另一个未解决的问题**，与核数无关。

## 工具用法

```sh
# 编译工具
cc -O2 -o apictool apictool.c
cc -O2 -o facptool facptool.c
cc -O2 -o gtdttool gtdttool.c

# 查看 MADT 结构
./apictool info

# 从原始转储重建并应用全部修复
./rebuild_apic.sh
```
