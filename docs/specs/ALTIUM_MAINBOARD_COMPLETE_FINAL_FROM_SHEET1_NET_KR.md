# Sheet1.NET 기준 메인보드 최종 인계 문서 (연결성 기준 재작성판)

이 문서는 **`Sheet1.NET`만을 source of truth**로 사용해서 다시 작성한 메인보드 최종 문서입니다.

가장 중요한 원칙은 아래 두 가지입니다.

1. **기존 문서의 의도나 설명보다 `Sheet1.NET`의 실제 연결을 우선**합니다.
2. 이 문서는 **연결성(net connectivity) 문서**입니다.
   따라서 netlist에 드러나지 않는 정보(커넥터 물리 방향, 실크 라벨 해석, 회로 의도, 배치 규칙)는 가능한 한 추측하지 않고 적었습니다.

---

## 0. 이 문서가 바로 고치는 핵심 사항

기존 `ALTIUM_MAINBOARD_COMPLETE_FINAL` 계열 문서와 비교했을 때, `Sheet1.NET` 기준으로 반드시 수정되어야 하는 포인트는 아래입니다.

- 메인보드 UWB는 **`U16 = DWM3000TR13`** 입니다. 기존 문서의 `U2` 기준이 아닙니다.
- 메인 3.3V rail 이름은 **`3.3V`** 입니다. 기존 문서의 `3.3V_EXT` 기준이 아닙니다.
- IMU의 실제 활성 커넥터는 **`J_IMU_1`** 입니다.
  `J_IMU_2`는 이 netlist에서 **`pin 1 = GND`만 연결**되어 있습니다.
- Tool satellite 마이크는 **`IC1-17`** 으로 들어갑니다.
  기존 문서처럼 `A0/A1/A2`로 Tool/L/R 세 채널을 바로 받는 구조가 아닙니다.
- Left / Right satellite의 pin 9는 **바로 Teensy ADC로 들어가지 않고**,
  각각 **`U30`, `U31` 아날로그 체인**으로 들어갑니다.
- 이 보드는 기존 문서보다 훨씬 큰 오디오 체인을 포함합니다.
  실제 netlist에는 **`PCM1808` 2개, `PCM5102A` 3개, 다수의 op-amp / multiplier 블록**이 있습니다.

---

## 1. 메인 active 부품 인벤토리

| 블록 | RefDes | 부품 |
| --- | --- | --- |
| MCU | `IC1` | `TEENSY41 / TEENSY_4.1` |
| 메인 UWB | `U16` | `DWM3000TR13` |
| 3.3V 전원 | `U1` + `U2` | `TPS62842DGRR`, `TPS7A2033PDBVR` |
| 음전원 생성 | `U3` + `U4` + `U5` | `LM27761DSGR`, `TPS63700DRCTG4`, `LT1964ES5-5#TRMPBF` |
| ADC | `U14`, `U17` | `PCM1808PWR` |
| DAC | `U15`, `U26`, `U27` | `PCM5102APWR` |
| OPA4192 | `U6`, `U7`, `U20`, `U21` | `OPA4192IPWR` |
| OPA2192 | `U12`, `U19`, `U28`, `U30`, `U31`, `U32` | `OPA2192IDR` |
| OPA1656 | `U13`, `U18` | `OPA1656IDR` |
| AD835 | `U8`~`U11`, `U22`~`U25` | `AD835ARZ-REEL7` |

---

## 2. 전원 레일 (netlist 기준)

| 전원 net | 의미 | 대표 연결 대상 |
| --- | --- | --- |
| `5V` | 입력 전원 / 양전원 아날로그 블록 공급 | `IC1-55`, `J1-1`, `U1`, `U3`, `U4`, `U6`, `U7`, `U12`, `U13`, `U14`, `U17`, `U18`, `U19`, `U20`, `U21`, `U28`, `U30`, `U31`, `U32` |
| `3.3V` | 메인 3.3V 레일 | `U2-5`, `U14-4`, `U15-1/8/20`, `U16-5/6/7`, `U17-4`, `U26-1/8/20`, `U27-1/8/20`, `J9-1`, `J10-1`, `J_SATELLITE_* -1`, `R59-2` |
| `3.3V_MCU` | Teensy 전용/직결 3.3V 레일 | `IC1-42`, `IC1-53`, `J_IMU_1-1` |
| `-3.3V` | LM27761 출력 레일 | `U3-4`, `J5-1` |
| `-5V` | LT1964 출력 레일 | `U5-5`, `J8-1`, `U6/U7/U12/U13/U18/U19/U20/U21/U28/U30/U31/U32` 일부 전원 |
| `GND` | 공통 접지 | `IC1-45/48/49/69/70/71`, `U16-2/8/16/21/23/24`, `J_IMU_1-3`, `J_IMU_2-1`, `J9-3`, `J10-3`, `J_SATELLITE_* -2` 등 |

### 2.1 해석 메모
- `5V`는 메인 입력 전원이며 `IC1-55`에도 직접 연결됩니다.
- `3.3V`는 메인 3.3V rail이며, satellite 헤더 3개와 `U16`, `PCM1808`, `PCM5102A` 블록들에 공급됩니다.
- `3.3V_MCU`는 별도 rail로 존재하며, 이 netlist에서는 **Teensy (`IC1-42`, `IC1-53`)와 IMU 전원 (`J_IMU_1-1`)** 에만 연결됩니다.
- `-3.3V`, `-5V`가 실제로 존재합니다. 즉 이 보드는 단순 3.3V MCU/UWB 보드가 아니라 **양전원 아날로그 체인**을 포함한 보드입니다.

---

## 3. Teensy (`IC1`) 실제 사용 핀

> 아래 표는 `Sheet1.NET`에서 **실제로 등장하는 `IC1-*` 핀만** 정리한 것입니다.
> 기존 문서에 있던 `IC1-14`, `IC1-15`, `IC1-16`, `IC1-18` 등은 이 netlist 기준으로는 사용되지 않습니다.

| Teensy 핀 | 실제 연결 |
| --- | --- |
| `IC1-1` | IC1-1 -> U16-19 + J_SATELLITE_TOOL1-5 + J_SATELLITE_L1-5 + J_SATELLITE_R1-5 |
| `IC1-2` | IC1-2 -> U16-3 (RSTn), R59-1; R59-2는 3.3V |
| `IC1-3` | IC1-3 -> U16-22 (IRQ), R60-2; R60-1은 GND |
| `IC1-4` | IC1-4 -> J_SATELLITE_L1-6 |
| `IC1-5` | IC1-5 -> U14-9 |
| `IC1-6` | IC1-6 -> J_SATELLITE_L1-8 |
| `IC1-7` | IC1-7 -> U15-14 |
| `IC1-8` | IC1-8 -> U17-9 |
| `IC1-9` | IC1-9 -> U26-14 |
| `IC1-10` | IC1-10 -> J_IMU_1-7 |
| `IC1-11` | IC1-11 -> J_IMU_1-5 |
| `IC1-12` | IC1-12 -> J_IMU_1-6 |
| `IC1-13` | IC1-13 -> J_IMU_1-4 |
| `IC1-17` | IC1-17 -> R61-2 + C132-1 (Tool satellite mic 경로) |
| `IC1-20` | IC1-20 -> U14-7 + U15-15 + U17-7 + U26-15 + U27-15 |
| `IC1-21` | IC1-21 -> U14-8 + U15-13 + U17-8 + U26-13 + U27-13 |
| `IC1-23` | IC1-23 -> U14-6 + U15-12 + U17-6 + U26-12 + U27-12 |
| `IC1-24` | IC1-24 -> J_SATELLITE_TOOL1-6 |
| `IC1-25` | IC1-25 -> J_SATELLITE_TOOL1-7 |
| `IC1-26` | IC1-26 -> U16-18 + J_SATELLITE_TOOL1-4 + J_SATELLITE_L1-4 + J_SATELLITE_R1-4 |
| `IC1-27` | IC1-27 -> U16-20 + J_SATELLITE_TOOL1-3 + J_SATELLITE_L1-3 + J_SATELLITE_R1-3 |
| `IC1-28` | IC1-28 -> J_SATELLITE_TOOL1-8 |
| `IC1-29` | IC1-29 -> J_SATELLITE_L1-7 |
| `IC1-30` | IC1-30 -> J_SATELLITE_R1-8 |
| `IC1-32` | IC1-32 -> U27-14 |
| `IC1-36` | IC1-36 -> J_SATELLITE_R1-6 |
| `IC1-37` | IC1-37 -> J_SATELLITE_R1-7 |
| `IC1-57` | IC1-57 -> U16-17 |

### 3.1 디지털 오디오 관련 Teensy 핀
아래 8개 핀은 UWB / IMU / satellite 제어가 아니라 **오디오 codec 체인**에 사용됩니다.

- `IC1-5  -> U14-9`
- `IC1-7  -> U15-14`
- `IC1-8  -> U17-9`
- `IC1-9  -> U26-14`
- `IC1-20 -> U14/U15/U17/U26/U27 공통`
- `IC1-21 -> U14/U15/U17/U26/U27 공통`
- `IC1-23 -> U14/U15/U17/U26/U27 공통`
- `IC1-32 -> U27-14`

netlist만으로는 이 핀들의 Teensy 논리 기능명을 확정하지 않고, **원문 그대로 pin number 기준**으로 유지하는 것이 안전합니다.

---

## 4. 메인보드 UWB (`U16 = DWM3000TR13`) 최종 연결표

| U16 핀 | 기능 | 실제 연결 |
| --- | --- | --- |
| 1 | EXTON | 연결 없음 |
| 2 | WAKEUP | `GND` |
| 3 | RSTn | `IC1-2`, `R59-1`; `R59-2 -> 3.3V` (`R59 = 10k`) |
| 4 | GPIO7/SYNC | 연결 없음 |
| 5 | VDD1 | `3.3V` |
| 6 | VDD3V3 | `3.3V` |
| 7 | VDD3V3 | `3.3V` |
| 8 | GND | `GND` |
| 9 | GPIO6/SPIPHA | `R63-1`; `R63-2 -> GND` (`R63 = 10k`) |
| 10 | GPIO5/SPIPOL | `R62-2`; `R62-1 -> GND` (`R62 = 10k`) |
| 11 | GPIO4 | 연결 없음 |
| 12 | GPIO3/TXLED | 연결 없음 |
| 13 | GPIO2/RXLED | 연결 없음 |
| 14 | GPIO1/SFDLED | 연결 없음 |
| 15 | GPIO0/RXOKLED | 연결 없음 |
| 16 | GND | `GND` |
| 17 | SPICSn | `IC1-57` |
| 18 | SPIMOSI | `IC1-26` |
| 19 | SPIMISO | `IC1-1` |
| 20 | SPICLK | `IC1-27` |
| 21 | GND | `GND` |
| 22 | GPIO8/IRQ | `IC1-3`, `R60-2`; `R60-1 -> GND` (`R60 = 100k`) |
| 23 | GND | `GND` |
| 24 | GND | `GND` |

### 4.1 `U16` 주변 수동소자
- `R59 = 10kΩ` : `U16 pin3 (RSTn)` pull-up to `3.3V`
- `R60 = 100kΩ` : `U16 pin22 (IRQ)` pull-down to `GND`
- `R62 = 10kΩ` : `U16 pin10` pull-down to `GND`
- `R63 = 10kΩ` : `U16 pin9` pull-down to `GND`

---

## 5. IMU 커넥터 (`J_IMU_1`, `J_IMU_2`) — netlist 그대로 정리

기존 문서와 달리, 이 netlist에서는 **실제 IMU 신호가 `J_IMU_1`에 연결**됩니다.

| 커넥터 핀 | 실제 연결 |
| --- | --- |
| `J_IMU_1-1` | `3.3V_MCU` |
| `J_IMU_1-3` | `GND` |
| `J_IMU_1-4` | `IC1-13` |
| `J_IMU_1-5` | `IC1-11` |
| `J_IMU_1-6` | `IC1-12` |
| `J_IMU_1-7` | `IC1-10` |
| `J_IMU_2-1` | `GND` |

### 5.1 결론
- **IMU 전원 / SPI / CS는 `J_IMU_1`에 있습니다.**
- `J_IMU_2`는 이 netlist에서 **`pin 1 = GND`만 연결**됩니다.
- 따라서 기존 문서의 “`J_IMU_2`가 활성 9핀 헤더”라는 설명은 이 netlist와 맞지 않습니다.
- 커넥터 물리 방향/실크 해석은 netlist만으로 확정할 수 없으므로, **회로도 원본 또는 PCB에서 별도 확인**해야 합니다.

---

## 6. Satellite 3개 헤더 최종 연결

### 6.1 `J_SATELLITE_TOOL1`
| 핀 | 실제 연결 |
| --- | --- |
| 1 | `3.3V` |
| 2 | `GND` |
| 3 | `IC1-27` |
| 4 | `IC1-26` |
| 5 | `IC1-1` |
| 6 | `IC1-24` |
| 7 | `IC1-25` |
| 8 | `IC1-28` |
| 9 | `R61-1` (아날로그 입력) |

### 6.2 `J_SATELLITE_L1`
| 핀 | 실제 연결 |
| --- | --- |
| 1 | `3.3V` |
| 2 | `GND` |
| 3 | `IC1-27` |
| 4 | `IC1-26` |
| 5 | `IC1-1` |
| 6 | `IC1-4` |
| 7 | `IC1-29` |
| 8 | `IC1-6` |
| 9 | `C149-2` (아날로그 입력) |

### 6.3 `J_SATELLITE_R1`
| 핀 | 실제 연결 |
| --- | --- |
| 1 | `3.3V` |
| 2 | `GND` |
| 3 | `IC1-27` |
| 4 | `IC1-26` |
| 5 | `IC1-1` |
| 6 | `IC1-36` |
| 7 | `IC1-37` |
| 8 | `IC1-30` |
| 9 | `C136-2` (아날로그 입력) |

### 6.4 공통 해석
- 세 satellite 헤더 모두 `pin 1 = 3.3V`, `pin 2 = GND`, `pin 3 = IC1-27`, `pin 4 = IC1-26`, `pin 5 = IC1-1` 구조를 공유합니다.
- Tool/Left/Right 차이는 `pin 6/7/8/9`에만 있습니다.
- `pin 9`는 세 보드 모두 아날로그 성격의 입력이지만, **Tool / Left / Right의 내부 처리 방식이 서로 다릅니다.**

---

## 7. Satellite 마이크 / 보정 마이크 실제 아날로그 경로

### 7.1 Tool satellite 아날로그 경로
`J_SATELLITE_TOOL1-9 -> R61-1 -> R61-2/P17 -> IC1-17`
동일 노드에 `C132-1`이 연결되고 `C132-2`는 `GND`입니다.

즉 Tool satellite pin 9는:
- `R61 = 1.5kΩ` 직렬
- `C132 = 1nF` to GND
- 최종적으로 `IC1-17`

로 들어갑니다.

### 7.2 Left satellite 아날로그 경로
`J_SATELLITE_L1-9 -> C149-2 -> C149-1 / R75-2 / U30-3`

관련 연결:
- `C149 = 1uF`
- `R75-1 -> GND`, `R75-2 -> U30-3` 쪽
- `U30 = OPA2192IDR`
- `U30-7` 출력은 `L_Mic`
- `L_Mic`는 `R54-1`, `R74-2`, `U30-7`에 연결됨

즉 Left satellite pin 9는 **직접 Teensy ADC로 가지 않고 `U30` 아날로그 체인으로 들어갑니다.**

### 7.3 Right satellite 아날로그 경로
`J_SATELLITE_R1-9 -> C136-2 -> C136-1 / R65-2 / U31-3`

관련 연결:
- `C136 = 1uF`
- `R65-1 -> GND`, `R65-2 -> U31-3` 쪽
- `U31 = OPA2192IDR`
- `U31-7` 출력은 `R_Mic`
- `R_Mic`는 `R30-1`, `R76-2`, `U31-7`에 연결됨

즉 Right satellite pin 9도 **직접 Teensy ADC로 가지 않고 `U31` 아날로그 체인으로 들어갑니다.**

### 7.4 별도 calibration mic 헤더
이 netlist에는 satellite 3개 외에 **별도 5핀 헤더 `J9`, `J10`** 가 있습니다.

#### `J9`
- `J9-1 -> 3.3V`
- `J9-3 -> GND`
- `J9-4 -> Left_Cal_Mic`
- `J9-5 -> C141-2`

이 경로는 `U28 = OPA2192IDR`와 연결됩니다.

#### `J10`
- `J10-1 -> 3.3V`
- `J10-3 -> GND`
- `J10-4 -> Right_Cal_Mic`
- `J10-5 -> C152-2`

이 경로는 `U32 = OPA2192IDR`와 연결됩니다.

---

## 8. 실제로 존재하는 오디오 codec 블록

기존 문서에는 거의 반영되지 않았지만, `Sheet1.NET`에는 아래 codec 블록이 실제로 존재합니다.

| RefDes | 부품 | 요약 |
| --- | --- | --- |
| `U14` | `PCM1808PWR` | `IC1-5,20,21,23`와 연결된 ADC |
| `U17` | `PCM1808PWR` | `IC1-8,20,21,23`와 연결된 ADC |
| `U15` | `PCM5102APWR` | `IC1-7,20,21,23`와 연결된 DAC |
| `U26` | `PCM5102APWR` | `IC1-9,20,21,23`와 연결된 DAC |
| `U27` | `PCM5102APWR` | `IC1-32,20,21,23`와 연결된 DAC |

### 8.1 codec 공통 digital bus
다음 3개 Teensy 핀은 여러 codec에 공통으로 묶여 있습니다.

- `IC1-20`
- `IC1-21`
- `IC1-23`

즉 `Sheet1.NET` 기준 이 보드는 단순 “Teensy + UWB + IMU + satellite 3개”가 아니라,
**오디오 ADC/DAC 서브시스템이 이미 들어간 통합 보드**입니다.

---

## 9. 외부로 나가는 named analog / audio net

| 커넥터 | net |
| --- | --- |
| `J52-1` | `.Left_DAC1` |
| `J50-1` | `.Left_DAC2` |
| `J53-1` | `.Right_DAC1` |
| `J51-1` | `.Right_DAC2` |
| `J48-1` | `.Left_Speech` + `NetJ48_1` |
| `J49-1` | `.R_Speech` |
| `J54-1` | `L_Speech` |
| `J55-1` | `R_Speech` |
| `J33-1` | `L_OUT` |
| `J30-1` | `R_OUT` |

### 9.1 해석 메모
- `J52/J50/J53/J51`은 DAC 출력 계열 net을 외부로 빼는 단일 핀 헤더입니다.
- `J48/J49/J54/J55`는 speech 계열 net을 외부로 빼는 단일 핀 헤더입니다.
- `J30/J33`은 `R_OUT`, `L_OUT` 단일 핀 헤더입니다.

---

## 10. 회로도 입력 / 검증 순서 (실무 권장)

### STEP 1 — 전원 rail부터 고정
먼저 아래 net 이름을 그대로 사용합니다.

- `5V`
- `3.3V`
- `3.3V_MCU`
- `-3.3V`
- `-5V`
- `GND`

기존 문서의 `3.3V_EXT` 같은 이름으로 바꾸지 않는 것이 안전합니다.

### STEP 2 — `IC1`와 `U16` 먼저 확정
- `IC1-1/2/3/24/25/26/27/28/29/30/36/37/57`
- `U16 pin 3/17/18/19/20/22`
- `R59`, `R60`, `R62`, `R63`

### STEP 3 — IMU는 `J_IMU_1` 기준으로 배선
- `J_IMU_1-1 -> 3.3V_MCU`
- `J_IMU_1-3 -> GND`
- `J_IMU_1-4/5/6/7 -> IC1-13/11/12/10`

### STEP 4 — satellite 3개는 9핀 구조 그대로 입력
- Tool: `24/25/28`
- Left: `4/29/6`
- Right: `36/37/30`

### STEP 5 — Tool/Left/Right 아날로그 경로를 서로 다른 블록으로 취급
- Tool pin 9: `R61 + C132 -> IC1-17`
- Left pin 9: `C149 + R75 + U30`
- Right pin 9: `C136 + R65 + U31`

### STEP 6 — 오디오 codec 블록은 별도 묶음으로 유지
- `U14`, `U17` = ADC
- `U15`, `U26`, `U27` = DAC
- 공통 digital bus = `IC1-20/21/23`

---

## 11. 꼭 기억해야 할 체크포인트

1. 메인 UWB refdes는 **`U16`** 입니다.
2. satellite 공통 전원은 **`3.3V`** 입니다. `3.3V_MCU`가 아닙니다.
3. IMU는 **`J_IMU_1`이 활성**, `J_IMU_2`는 사실상 비활성입니다.
4. Tool satellite 아날로그 입력은 **`IC1-17`** 로 갑니다.
5. Left/Right satellite 아날로그 입력은 **직접 Teensy로 가지 않고 `U30`/`U31`로 들어갑니다.**
6. 이 보드에는 실제로 **ADC 2개 + DAC 3개 + 양전원 아날로그 체인**이 포함됩니다.
7. 따라서 기존의 “Teensy + UWB + IMU + satellite 3개만 있는 단순 메인보드”라는 문서 구조로는 **이 netlist를 완전히 설명할 수 없습니다.**

---

## 12. 한 줄 결론

`Sheet1.NET` 기준 최종 문서는 다음처럼 이해하는 것이 맞습니다.

**이 보드는 `Teensy 4.1 + DWM3000(U16) + IMU(J_IMU_1) + 3개 satellite 헤더 + 별도 calibration mic 헤더(J9/J10) + 다수의 audio ADC/DAC/analog block`으로 이루어진 통합 메인보드입니다.**

기존 `ALTIUM_MAINBOARD_COMPLETE_FINAL` 문서는 이 중 **UWB/satellite 제어 remap 일부만 맞고**,
IMU / satellite analog / audio block / refdes / power naming은 `Sheet1.NET` 기준으로 다시 써야 합니다.
