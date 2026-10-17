# Cycle-Invariant AEAD & Silicon-Jitter TRNG Engine
**Target Platform:** STM32C031 (ARM Cortex-M0+ @ 48 MHz)  
**Architecture:** Zero-Library Bare-Metal CMSIS Register Manipulation  

---

## Abstract
This repository contains the complete artifacts, bare-metal C source code, and empirical benchmark logs for a cycle-invariant authenticated encryption and silicon-jitter true random number generator (TRNG) architecture implemented on an STM32C031 (ARM Cortex-M0+). Designed for resource-constrained edge systems (such as Electric Vehicle BMS nodes), the system achieves strict constant-time execution ($\Delta = 0$ clock cycle variance) to defeat timing side-channel attacks, harvests cross-domain clock phase jitter whitened via the hardware CRC-32 unit, and implements sub-microsecond interrupt-driven SRAM tamper key-shredding.

---

## 1. Introduction & Problem Statement
Ultra-low-cost microcontrollers deployed in distributed automotive and IoT nodes frequently lack native hardware TRNGs and hardware AES accelerators. Standard software implementations are computationally expensive, consume excessive flash memory, and leak execution timing variations that allow attackers to recover secret keys via side-channel analysis.

---

## 2. Proposed Bare-Metal Architecture
The proposed architecture operates via direct CMSIS register manipulation without software libraries or operating systems:
* **Silicon-Jitter TRNG:** Harvests phase drift between the core `SysTick` timer and peripheral `TIM3` clock mixed with `ADC1` SAR quantization noise, whitened through the on-chip `CRC->DR` polynomial engine.
* **Constant-Time AEAD Pipeline:** Implements a 32-round Feistel block cipher combined with a keyed Hardware CRC-32 MAC tag, utilizing zero data-dependent branches.
* **Tamper Key-Shredding:** Employs a RAM-relocated vector table (`SCB->VTOR`) and an `EXTI0` falling-edge hardware interrupt to execute a 2-pass SRAM entropy-flooding and zeroization sequence upon physical intrusion.

---

## 3. Empirical Benchmarks & Comparative Analysis
The system was benchmarked on an ARM Cortex-M0+ running at 48 MHz:
* **Cycle Invariance:** Verified via ARM `SysTick->VAL` profiling, proving a steady-state execution time of exactly **11,988 CPU cycles (249.75 µs)** with zero variance ($\sigma^2 = 0$), compared to variable-timing table-based software ciphers (~35,000+ cycles).
* **Memory Footprint:** Consumes `< 1.2 KB` of Flash and `< 64 bytes` of SRAM, outperforming bulky software stacks like mbedTLS.
* **Tamper Latency:** Achieves key-shredding in `< 1 µs`, mitigating cold-boot memory retention.

---

## 4. Repository & File Structure
* **`main.c`:** Contains the bare-metal C implementation, register initializations, TRNG entropy harvester, constant-time cipher, and `EXTI0` tamper interrupt service routine.
* **`diagram.json`:** The Wokwi hardware simulation configuration file defining the STM32C031 Nucleo board, the analog potentiometer sensor (`PA1`), and the physical tamper pushbutton (`PA0`).
* **`aead_out.png`:** Empirical benchmark serial output log demonstrating live sequence tracking, encrypted ciphertext, hardware CRC-32 verification tags, and cycle-count invariance.

---

## Visual Demonstration
![Benchmark Output](aead_out.png)

---

## Technical Report
The project has been published as a technical report / preprint on Zenodo, to which the link is provided below:  
https://doi.org/10.5281/zenodo.23441546

---

## Citation
For now, cite the Zenodo preprint and software record:
```bibtex
@misc{swornampillai2026cycleinvariant,
      title={Cycle-Invariant Lightweight Authenticated Encryption and Cross-Domain Silicon Entropy Harvesting on ARM Cortex-M0+}, 
      author={Vasudevan Swornampillai},
      year={2026},
      month={October},
      publisher={Zenodo},
      doi={10.5281/zenodo.23441546},
      url={[https://doi.org/10.5281/zenodo.23441546](https://doi.org/10.5281/zenodo.23441546)}
}
