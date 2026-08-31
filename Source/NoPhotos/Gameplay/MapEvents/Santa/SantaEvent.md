# 산타 이벤트: 비행 경로와 선물 투하

산타 생성/직선 비행/종료와 함께, 경로 위에서 선물상자를 투하하고 착지 후 개봉해 랜덤 유물을 생성합니다. **UI, 사진 판정, 기존 SpawnVolume이나 다른 이벤트는 변경하지 않습니다.** 선물 설정은 같은 폴더의 `SantaGift.md`를 참고하세요.

## 구성

| 클래스 | 역할 |
| --- | --- |
| `NPSantaMapEvent` | 기존 맵 이벤트 수명에 연결. 서버에서 경로 선택과 산타 생성/제거 |
| `NPSantaEventDefinition` | 기존 메타데이터/Duration + 산타 외형 클래스/경로 그룹 DA |
| `NPSantaFlightRoute` | 레벨에 배치하는 직선 경로 기준점. 에디터에 선/방향 화살표 표시 |
| `NPSantaFlightActor` | 산타 외형과 비행. 서버에서 받은 시작점/끝점/시작 시각/Duration으로 이동 |
| `FNPSantaFlightPlan` | 복제할 비행 데이터. 진행률과 위치 계산은 이 구조체에서 공유 |
| `NPSantaGiftActor` | 독립된 선물상자. 서버 낙하/착지, 개봉 연출, 랜덤 유물 1개 생성 |
| `FNPSantaGiftDropSchedule` | 비행 중 투하 수량과 진행 구간 설정 |

## 사용자가 에디터에서 설정할 것

빌드, BP/DA/레벨 생성 및 수정은 사용자가 진행합니다. 새 C++ 클래스가 반영된 에디터에서 아래를 설정하세요.

### 1. 산타 외형 BP

1. 부모 클래스를 **NPSantaFlightActor**로 하는 `BP_SantaFlight`를 만듭니다.
2. 상속된 `SleighMesh`에 썰매 Static Mesh, `SantaMesh`에 산타 Skeletal Mesh/애니메이션을 지정합니다. 하나만 사용해도 됩니다. 기본 외형 에셋은 코드에서 지정하지 않습니다.
3. 위치/크기/회전은 각 메시 또는 `VisualRoot`의 상대 Transform으로 조절합니다. 모델 정면은 비행 액터의 **로컬 +X**입니다. 모델이 뒤를 보면 `VisualRoot`의 Yaw를 조절하세요.
4. 추가 외형 컴포넌트도 `VisualRoot` 아래에 붙이고 충돌/물리 시뮬레이션을 끄세요. 기본 두 메시의 충돌/오버랩/Nav 영향은 코드에서 꺼두었습니다.
5. **Replicates / Always Relevant는 켜고, Replicate Movement는 끈 상태**를 유지합니다. BP의 Tick/Timeline/MovementComponent로 별도 이동시키지 않습니다.

### 2. 직선 경로 배치

메인 레벨 또는 카탈로그의 선택적 `Location Level Instance` 원본 레벨에 **NPSantaFlightRoute** 액터를 배치합니다. 별도 경로 BP는 필수가 아닙니다.

| 항목 | 의미 / 예시 |
| --- | --- |
| 액터 Location | 비행 구간의 기준 중심. X/Y를 바꾸면 경로 전체가 이동 |
| 액터 Rotation Yaw | 비행 방향. 0도는 월드 +X, 90도는 +Y, 180도는 -X |
| Flight Height | 기준 중심에서 월드 위쪽으로 더하는 높이. 기본 2000cm |
| Flight Distance | 시작점부터 끝점까지 전체 길이. 기본 12000cm |
| Route Group | 기본 `Santa`. 이벤트 DA와 정확히 같아야 함 |
| Selection Weight | 기본 1. 0이면 선택하지 않음 |

```text
중심 = 액터 위치 + (0, 0, FlightHeight)
방향 = 액터 Yaw가 향하는 수평 단위 벡터
시작 = 중심 - 방향 × FlightDistance / 2
끝   = 중심 + 방향 × FlightDistance / 2
```

- 에디터의 경로 선과 빨간 화살표로 배치를 확인합니다. 게임 중에는 표시하지 않습니다.
- 미리보기 Spline 점을 직접 옮기지 말고 **액터 위치/Yaw, Height, Distance**를 수정하세요. 점은 Construction에서 다시 계산합니다.
- Pitch/Roll/Scale은 비행 경로 계산에서 무시합니다. 액터 Scale은 1로 두고 길이는 Distance로 설정하는 것을 권장합니다.
- 지면 높이 탐색이나 장애물 회피는 없습니다. 건물/산을 지나지 않게 높이를 배치하고 확인하세요.
- Collector, SpawnPoint, SpawnVolume, NavMesh는 이 이벤트 경로에 필요하지 않습니다.
- 같은 그룹의 유효한 경로가 여러 개면 가중치로 하나를 고릅니다. **방향 자체는 랜덤이 아닙니다.**
- 현재 로드된 모든 레벨에서 해당 그룹을 검색합니다. 경로 세트를 분리하려면 별도 Gameplay Tag를 등록하고 DA/경로에 동일하게 지정하세요.

### 3. 이벤트 DA / 카탈로그

1. **NPSantaEventDefinition** 타입의 Data Asset을 만듭니다. 일반 `NPMapEventDefinition` DA에는 산타 설정이 없습니다.
2. `Event Class` = **NPSantaMapEvent**. 필요하면 사용자가 만든 해당 클래스 자식 BP를 지정해도 됩니다. Apply Event State를 BP에서 재정의하면 부모 호출을 유지해야 합니다.
3. `Santa Class` = 위에서 만든 **BP_SantaFlight**, `Route Group` = **Santa**.
4. 기존처럼 Event Id, Display Name, Type, Scale 등 메타데이터를 설정합니다.
5. `Duration`은 **전체 비행 시간**입니다. 예: **30초**. 0초/무제한은 지원하지 않으며 최소 0.01초입니다. 12000cm / 30초이면 속도는 400cm/s입니다.
6. 기존 `DA_EventCatalog`의 `Event Entries`에 이 DA와 이벤트 선택 가중치를 넣습니다.
7. 별도 위치 레벨을 사용한다면 해당 엔트리의 `Location Level Instance`에 경로 액터가 있는 레벨을 지정합니다. 메인 레벨에 이미 경로가 있으면 비워도 됩니다.
8. 선물 투하를 쓰려면 DA의 `Gift Class`, `Relic Classes`, `Gift Drops`를 설정합니다. 상세는 `SantaGift.md`를 참고하세요. `Gift Drops.Count = 0`이면 비행만 합니다.

기존 이벤트의 `Location Source`는 Point/Volume용 설정이며, 산타의 경로 선택에는 사용하지 않습니다.

## 실행 / 복제 / 정리

- 기존 매니저가 이벤트 시작 시점과 수명을 관리합니다. 서버는 로드된 경로 중 하나를 골라 **월드 시작점/끝점을 한 번 확정**합니다. 비행 중 경로 액터를 옮겨도 현재 비행은 바뀌지 않습니다.
- 산타는 서버의 Persistent Level에 생성됩니다. 경로 전용 위치 레벨이 서버에만 있어도 클라이언트는 복제된 계획만으로 비행합니다.
- 각 화면은 GameState의 서버 시각으로 진행률을 계산합니다. 늦게 접속해도 계획 수신 후 현재 진행률을 사용합니다. 별도 이동 복제와 이중 보간하지 않습니다.
- DA Duration 만료, 수동 FinishEvent, 이벤트 액터 제거 시 산타를 정리합니다. 클라이언트도 비행 종료 시각 이후에는 외형을 숨기고 서버의 제거 복제를 기다립니다.
- 설정 오류/경로 없음/생성 실패는 `LogNPSantaEvent` 경고 후 다음 틱에 종료합니다. 기본 클래스의 Started → Finished 통지 순서를 보존하기 위해 즉시 종료하지 않습니다.
- 산타가 비행 도중 외부에서 Destroy되면 이벤트도 다음 틱에 종료합니다.
- 이벤트는 같은 인스턴스에서 재시작할 수 있으며, 이전 산타/실패 종료 타이머는 먼저 정리합니다.
- 선물은 현재 서버 비행 진행률로 계산한 산타 위치 바로 아래에서 떨어집니다. 비행 종료 시 새 투하는 멈추지만 이미 떨어진 선물은 착지/개봉을 마치며, 생성된 유물도 남습니다. 클라이언트의 `GetSanta()`는 복제 도착 순서에 따라 잠시 null일 수 있습니다.

## 검증 안내

요청에 따라 빌드와 PIE/자동화 테스트 실행은 하지 않았습니다. `NPSantaFlightTests.cpp`에 순수 경로/진행률 계산 테스트를 추가했습니다. 사용자 빌드 후 Automation에서 `NoPhotos.MapEvents.Santa`로 실행할 수 있습니다.

에디터 실행 확인 항목:

1. 경로 위치 이동 / Yaw 0·90·임의 각도에서 미리보기와 산타 이동이 일치하는지.
2. Listen Server 및 Dedicated Server + 클라이언트 2개에서 진행 위치/방향이 일치하는지.
3. 비행 중 접속한 클라이언트가 시작점부터 다시 비행하지 않는지.
4. Duration 종료 / 수동 종료 / 재실행에서 이전 산타가 남거나 중복되지 않는지.
5. 그룹 불일치, 가중치 0, 산타 클래스 누락, Duration 0일 때 경고 후 종료되는지.
6. 서버 전용 Location Level Instance에 경로를 넣어도 클라이언트에서 산타가 보이는지.
7. 경로가 없는 상태에서 실패 종료 후 다음 이벤트가 정상 실행되는지.

이 검증은 아직 실행 결과가 아닌 사용자 확인 절차입니다.
