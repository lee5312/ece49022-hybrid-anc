# [Team 39] 통합 위성 보드 (Tool / L / R) 하드웨어 설계 명세서 — 수정본 v2

## 1. 개요
본 기판은 공간 인지형 능동 소음 제어 헤드셋의 **Tool / Left / Right** 위치에 공통으로 사용하는 위성 보드입니다.
각 위성 보드는 아래 두 기능만 담당합니다.

1. **UWB 모듈 1개 탑재**: Qorvo **DWM3000TR13**
2. **외부 마이크 인터페이스 1개 제공**: **Adafruit SPW2430 MEMS Microphone Breakout** 장착용 **5핀 헤더**

메인보드(Teensy)와는 **9핀 케이블 1개**로 연결합니다.
이 문서는 **알티움 회로도/PCB 작성용 기준 문서**이며, DWM3000 핀맵 오류를 반영해 수정한 버전입니다.

---

## 2. 부품 목록 (BOM, 보드 1장 기준)

### 2.1 핵심 부품
- `UWB_MOD` : **DWM3000TR13** (Qorvo UWB Transceiver Module) × 1
- `J_MIC` : **1x5 헤더 풋프린트** × 1
  - 용도: **Adafruit SPW2430 MEMS Microphone Breakout** 장착
  - **J_MIC 핀 번호 기준:**
    - `Pin 1 = Vin`
    - `Pin 2 = 3V`
    - `Pin 3 = GND`
    - `Pin 4 = AC`
    - `Pin 5 = DC`
  - 즉, **브레이크아웃 실크 인쇄 순서 `Vin / 3V / GND / AC / DC`를 J_MIC 1번부터 5번까지 그대로 대응**시킵니다.
- `J1` : **1x9 헤더/커넥터** × 1
  - 용도: 메인보드 연결

### 2.2 수동 소자
- `C1` : **100nF, 0402, MLCC** × 1
  - DWM3000 전원 디커플링
- `R1` : **10kΩ, 0402** × 1
  - DWM3000 `RSTn` 풀업
- `R2` : **100kΩ, 0402** × 1
  - DWM3000 `IRQ/GPIO8` 풀다운

> 참고: 마이크용 RC 저역통과필터(예: 1.5kΩ + 10nF)는 **메인보드의 Teensy ADC 입력 근처**에 둡니다.
> 따라서 위성 보드에는 마이크 출력용 아날로그 필터 소자를 넣지 않습니다.

---

## 3. 시스템 인터페이스 요약

### 3.1 9핀 메인 커넥터 J1

| J1 핀 | 신호명 | 위성 보드 내부 연결 |
|---|---|---|
| 1 | `3.3V_EXT` | DWM3000 `VDD1`(5), `VDD3V3`(6,7), R1 한쪽, `J_MIC Pin 1 (Vin)` |
| 2 | `GND` | DWM3000 `VSS`(8,16,21,23,24), `WAKEUP`(2), R2 한쪽, C1 한쪽, `J_MIC Pin 3 (GND)` |
| 3 | `SPI1_SCK` | DWM3000 `SPICLK`(20) |
| 4 | `SPI1_MOSI` | DWM3000 `SPIMOSI`(18) |
| 5 | `SPI1_MISO` | DWM3000 `SPIMISO`(19) |
| 6 | `UWB_CS` | DWM3000 `SPICSn`(17) |
| 7 | `UWB_RST` | DWM3000 `RSTn`(3), R1 통해 `3.3V_EXT` 풀업 |
| 8 | `UWB_IRQ` | DWM3000 `GPIO8/IRQ`(22), R2 통해 GND 풀다운 |
| 9 | `MIC_OUT` | `J_MIC Pin 5 (DC)` |

### 3.2 마이크 헤더 J_MIC (Adafruit SPW2430 Breakout 장착용)

**중요:** 이 보드는 SPW2430 칩을 직접 실장하지 않고, **Adafruit SPW2430 breakout 보드 전체를 5핀 헤더로 꽂는 방식**입니다.

| J_MIC 핀 번호 | J_MIC 핀명 | 연결 대상 | 사용 여부 | 비고 |
|---|---|---|---|---|
| 1 | `Vin` | `3.3V_EXT` | 사용 | 브레이크아웃 입력 전원 |
| 2 | `3V` | N/C | 미사용 | 브레이크아웃 내부 레귤레이터 출력으로 간주, 외부에서 구동하지 않음 |
| 3 | `GND` | `GND` | 사용 | 공통 접지 |
| 4 | `AC` | N/C | 미사용 | AC-coupled 출력. 본 설계에서는 사용하지 않음 |
| 5 | `DC` | `MIC_OUT` | 사용 | Teensy ADC로 가는 아날로그 출력 |

### 3.3 마이크 헤더 방향성 주의
헤더 풋프린트는 **실제 Adafruit breakout의 실크 인쇄 순서 `Vin / 3V / GND / AC / DC`와 정확히 일치하도록** 배치해야 합니다.
즉 **J_MIC Pin 1 → Vin, Pin 2 → 3V, Pin 3 → GND, Pin 4 → AC, Pin 5 → DC**가 되어야 합니다.
특히 알티움에서 **보드 상면/하면 전환 시 핀 순서가 좌우 반전**되지 않도록 반드시 1회 실물 기준으로 재확인하십시오.

---

## 4. DWM3000 정확한 핀 연결표 (수정 완료)

아래 표는 **Qorvo DWM3000 데이터시트 기준의 실제 핀맵**입니다.
이 표를 기준으로 알티움 심볼/풋프린트를 작성하십시오.

| DWM3000 핀 | 신호명 | 연결 대상 | 비고 |
|---|---|---|---|
| 1 | `EXTON` | N/C | 미사용 |
| 2 | `WAKEUP` | GND | 슬립 기능 미사용 시 tie-low |
| 3 | `RSTn` | `UWB_RST` | R1(10kΩ)으로 `3.3V_EXT` 풀업 |
| 4 | `GPIO7/SYNC` | N/C | 미사용 |
| 5 | `VDD1` | `3.3V_EXT` | **필수 연결** |
| 6 | `VDD3V3` | `3.3V_EXT` | **필수 연결** |
| 7 | `VDD3V3` | `3.3V_EXT` | **필수 연결** |
| 8 | `VSS` | GND | **필수 연결** |
| 9 | `GPIO6` | N/C | 미사용 |
| 10 | `GPIO5` | N/C | 미사용 |
| 11 | `GPIO4` | N/C | 미사용 |
| 12 | `GPIO3` | N/C | 미사용 |
| 13 | `GPIO2` | N/C | 미사용 |
| 14 | `GPIO1` | N/C | 미사용 |
| 15 | `GPIO0` | N/C | 미사용 |
| 16 | `VSS` | GND | **필수 연결** |
| 17 | `SPICSn` | `UWB_CS` | Active-low |
| 18 | `SPIMOSI` | `SPI1_MOSI` | SPI 입력 |
| 19 | `SPIMISO` | `SPI1_MISO` | SPI 출력 |
| 20 | `SPICLK` | `SPI1_SCK` | SPI 클럭 |
| 21 | `VSS` | GND | **필수 연결** |
| 22 | `GPIO8/IRQ` | `UWB_IRQ` | R2(100kΩ)로 GND 풀다운 |
| 23 | `VSS` | GND | **필수 연결** |
| 24 | `VSS` | GND | **필수 연결** |

---

## 5. 회로 연결 요약

### 5.1 DWM3000 전원
- `VDD1`(5) → `3.3V_EXT`
- `VDD3V3`(6,7) → `3.3V_EXT`
- `VSS`(8,16,21,23,24) → `GND`
- `C1 = 100nF`는 `3.3V_EXT` ↔ `GND`로 배치하고, 가능한 한 DWM3000 전원 핀 가까이에 둡니다.

### 5.2 DWM3000 제어선
- `RSTn`(3) → `UWB_RST`
- `RSTn`에는 `R1 = 10kΩ`로 `3.3V_EXT` 풀업
- `GPIO8/IRQ`(22) → `UWB_IRQ`
- `GPIO8/IRQ`에는 `R2 = 100kΩ`로 GND 풀다운
- `WAKEUP`(2) → GND

### 5.3 DWM3000 SPI
- `SPICLK`(20) → `SPI1_SCK`
- `SPIMOSI`(18) → `SPI1_MOSI`
- `SPIMISO`(19) → `SPI1_MISO`
- `SPICSn`(17) → `UWB_CS`

### 5.4 마이크 브레이크아웃
- `J_MIC Pin 1 (Vin)` → `3.3V_EXT`
- `J_MIC Pin 2 (3V)` → N/C
- `J_MIC Pin 3 (GND)` → `GND`
- `J_MIC Pin 4 (AC)` → N/C
- `J_MIC Pin 5 (DC)` → `MIC_OUT`

---

## 6. PCB 레이아웃 제약 사항

1. **UWB 안테나 keep-out 준수**
   DWM3000 안테나 영역 아래에는 copper, trace, via, plane을 두지 않습니다.

2. **디커플링 캡 최단 거리 배치**
   `C1`은 DWM3000 전원 핀군(5/6/7)에 가능한 한 가깝게 둡니다.

3. **RF GND 충분히 확보**
   DWM3000 모듈 주변 GND stitching via를 충분히 배치합니다.

4. **마이크 아날로그 라우팅 분리**
   `MIC_OUT`은 SPI 신호선 및 DWM3000 안테나 부근을 피해서 배선합니다.

5. **마이크 헤더 방향 재확인**
   `J_MIC`는 breakout을 꽂았을 때 **Pin 1~5가 `Vin / 3V / GND / AC / DC` 순서로 정확히 대응**되도록 배치합니다.

6. **장거리 Tool 케이블 고려**
   Tool 위치에서 약 60 cm–1 m 케이블을 쓸 경우 SPI1 클럭은 **2 MHz 고정**을 전제로 하고, 실제 하네스 설계에서는 고속 SPI선이 GND 리턴에 가깝도록 배치합니다. `MIC_OUT`은 가능한 한 SPI선과 떨어뜨리십시오.

---

## 7. 알티움 입력용 핵심 체크리스트

### 반드시 맞아야 하는 항목
- DWM3000 전원 핀은 **5, 6, 7**이 전원
- DWM3000 GND 핀은 **8, 16, 21, 23, 24**
- `RSTn`은 10kΩ 풀업 필요
- `IRQ/GPIO8`은 100kΩ 풀다운 필요
- `WAKEUP`은 GND 고정
- 마이크 breakout은 **J_MIC Pin 5 (`DC`) 출력만 사용**
- 마이크 breakout의 **J_MIC Pin 2 (`3V`)는 입력으로 쓰지 않음**
- 마이크 헤더는 **Pin 1~5 = `Vin / 3V / GND / AC / DC`**

### 절대 틀리면 안 되는 포인트
- DWM3000에 **존재하지 않는 pin 25를 만들지 말 것**
- `VDD1`(핀 5)을 빠뜨리지 말 것
- 마이크 헤더 핀 순서를 미러링해서 뒤집지 말 것
- `J_MIC Pin 1`이 반드시 `Vin`이 되도록 할 것
- `J_MIC Pin 5`가 반드시 `DC`가 되도록 할 것

---

## 8. 최종 한 줄 요약
이 위성 보드는 **DWM3000 1개 + Adafruit SPW2430 breakout용 5핀 헤더 1개 + 메인보드용 9핀 커넥터 1개**로 구성하며,
가장 중요한 수정 사항은 **DWM3000 실제 핀맵 반영(특히 VDD1/VDD3V3/VSS)** 및 **J_MIC 1~5 핀 번호를 `Vin / 3V / GND / AC / DC`에 정확히 대응시키는 것**입니다.
