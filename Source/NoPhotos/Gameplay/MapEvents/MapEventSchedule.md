# 이벤트 사전 스케줄과 UI 데이터

## 설정

GameState의 Map Event Manager 컴포넌트에서 설정합니다. UI 위젯과 에셋은 변경하지 않습니다.

- `Start Automatically`: 켜면 서버의 매니저 BeginPlay에서 전체 목록을 미리 추첨합니다.
- `Map Event | Schedule → First Event Start Time Seconds`: 게임 시작(매니저 BeginPlay) 기준 첫 이벤트의 고정 시작 시간입니다. 기본 0초, 예를 들어 60이면 1분 후 시작합니다.
- `Minimum Event Count / Maximum Event Count`: 실행 개수를 범위 내에서 추첨합니다. 기본 2~3개입니다. 둘 다 0이면 자동 실행하지 않습니다. 역순 입력은 정렬하며, 사전 배열과 복제 크기 보호를 위해 0~256으로 제한합니다.
- 각 Definition의 `Duration`: 해당 이벤트의 진행 시간입니다.
- 각 Definition의 `Delay`: 해당 이벤트가 끝난 뒤 다음 이벤트까지 기다리는 시간입니다.

개수뿐 아니라 **이벤트 순서 전체를 시작 시 서버에서 확정**합니다. 카탈로그의 가중치를 사용하며, 기존처럼 초대형 이벤트와 가중치 0인 이벤트는 제외합니다. A/B 타입은 자동 순서를 제한하지 않습니다. 동일 Event Id는 한 번만 선택하며, ID가 없으면 Definition 에셋, Definition도 없으면 액터 클래스로 중복을 구분합니다. 서로 다른 종류라면 고유한 Event Id를 설정하세요. 실행 시 재추첨하지 않습니다.

이미 실행한 종류는 같은 매니저의 스케줄을 재시작하거나 수동 TriggerRandomEvent를 호출해도 다시 선택하지 않습니다. 수동 실행은 현재 자동 목록에 예약된 종류도 제외합니다. 고유한 후보가 설정 개수보다 적으면 남은 종류만 실행하고 `[MapEventPlan] 중복 제외 후 후보 부족` 로그를 남깁니다. 예를 들어 3개를 요청했지만 고유한 후보가 2종이면 2개만 실행합니다. 새 판은 새 GameState/매니저에서 시작합니다.

예: 첫 시작 60초, 첫 이벤트 Duration 20초, Delay 10초라면 두 번째 이벤트의 예상 시작은 90초입니다. 실제 실행은 첫 이벤트의 **실제 종료 + 10초**를 기다립니다. 위치 레벨 로드/언로드나 수동 이벤트 때문에 시작이 늦어질 수 있습니다. 실제 시작/종료 시 후속 예상 시각을 갱신합니다.

`Start Automatically`를 끈 경우 서버에서 `Start Event Scheduling`을 호출할 때 목록을 생성합니다. 이때도 첫 시작 기준은 매니저 BeginPlay이며, 지정 시각이 이미 지났다면 바로 시작 요청합니다. 스케줄이 실행 중이면 반복 호출해도 재추첨하지 않습니다.

## 서버 사전 안내 로그

서버 Output Log에서 `MapEventPlan`으로 검색하면 확정된 총 개수와 `[1/3]` 같은 순번, 이벤트 이름/ID, 예상 시작 시각, Duration, Delay를 실행 전에 확인할 수 있습니다. 마지막 이벤트가 끝나면 `Executed=3/3`과 완료 로그가 출력됩니다. 예상 시각이 늘어나거나 남은 게임 시간이 많아도 계획에 이벤트를 추가하지 않습니다. 수동 `TriggerRandomEvent`나 완료 후 명시적인 `StartEventScheduling` 재호출은 별도 실행입니다.

## BP에서 UI 연결

1. `Get Game State → Get Component by Class (NPMapEventManagerComponent)`로 매니저를 얻습니다. 아직 없으면 생성될 때까지 재시도합니다.
2. `Bind Event to On Event Schedule Changed`로 변경 알림을 연결합니다.
3. 바인딩 **직후 한 번**, 그리고 알림마다 `Get Event Schedule`을 읽습니다. 이렇게 하면 늦게 생성된 UI와 중도 접속도 현재 전체 목록을 표시할 수 있습니다.
4. 위젯 파괴 시 바인딩을 해제합니다. 클라이언트는 스케줄을 새로 생성하거나 추첨하지 않습니다.

| BP 함수 | 반환 값 |
| --- | --- |
| `Get Event Schedule` | 전체 목록 `Events`, 기준 서버 시각 `ScheduleOriginServerWorldTime`, 자동 실행 여부 `Running` |
| `Get Scheduled Event Presentations` | 실행 순서대로 정렬된 목록만 반환 |
| `Get Next Scheduled Event Presentation` | Pending 또는 Loading 중 첫 슬롯. 없으면 false |
| `Get Next Event Start Remaining Seconds` | 다음 이벤트의 예상 시작까지 남은 게임 시간. 없거나 예측 불가하면 -1 |
| `Get Schedule Elapsed Seconds` | 스케줄 기준 시각부터 경과한 게임 시간 |

각 목록 항목은 `Schedule Index`, `Event Id`, `Title`, `Description`, `Duration Seconds`, `Delay After Seconds`, 예상/실제 시작·종료 서버 시각, `State`를 제공합니다. 아이콘은 `Event Id`로 UI 데이터 테이블에서 조회할 수 있습니다. UI 목록의 순서는 **Schedule Index**를 사용합니다.

| State | 의미 |
| --- | --- |
| Pending | 실행 예정 |
| Loading | 해당 이벤트의 시작 요청/위치 레벨 준비 중 |
| Active | 실제 시작됨 |
| Completed | 실제 종료됨 |
| Cancelled | 스케줄 중지/시작 실패 등으로 취소됨 |

예상·실제 시각은 `GameState → Get Server World Time Seconds`와 같은 기준입니다. 아직 알 수 없는 시각은 **-1**입니다. Duration이 0인 이벤트는 수동 종료 전까지 끝을 예측할 수 없으므로 그 뒤의 예상 시각도 -1이며, 실제 종료되면 계산합니다.

프로그레스바의 이벤트 마커 위치는 다음처럼 계산할 수 있습니다. 예상 시작이 -1이면 위치를 임의로 0에 놓지 말고 별도 대기 목록 등으로 표시합니다.

```text
이벤트 시작 경과 초 = Expected Start Server World Time - Schedule Origin Server World Time
마커 비율 = Clamp(이벤트 시작 경과 초 / 게임 전체 시간, 0, 1)
```

변경 알림은 목록/상태/예상 시각이 바뀔 때 전달합니다. 남은 초를 매 프레임 복제하지 않으므로 카운트다운은 UI의 Tick/Timer에서 조회합니다. 타임 딜레이션(배속)은 게임 시간에 함께 적용됩니다.

`Stop Event Scheduling`은 미실행 슬롯을 Cancelled로 만들고 자동 실행을 중지합니다. 이미 Active인 이벤트는 계속 실행되고 종료 시 Completed로 갱신됩니다. 공개된 이벤트가 실행 불가능해지면 다른 이벤트로 재추첨하지 않고 스케줄을 중지합니다. 완료/취소 항목은 목록에 남아 UI에서 진행 기록을 표시할 수 있습니다. 진행 중 이벤트까지 끝난 뒤 명시적으로 다시 Start하면 새 목록을 생성합니다.

기존 `Get Active Event Presentations / On Active Map Events Changed`는 유지됩니다. 기존 UI는 그대로 사용할 수 있습니다.

## 게임 종료

`ANPMainGameState::FinishMainGame`에서 GameState의 모든 이벤트 매니저에 `Shutdown Events For Game End`를 호출합니다. 시간 만료와 기존 조기 종료 경로 모두 공통 적용됩니다. 미실행/로딩 슬롯을 취소하고 진행 중인 자동·수동 이벤트의 `FinishEvent`를 호출하여 각 이벤트의 종료 처리를 수행합니다. 관리 목록 밖의 레벨 배치 이벤트도 종료합니다. 활성 UI 목록은 비우고, 강제 종료된 슬롯은 Cancelled로 표시합니다.

매니저 종료 플래그와 게임 종료 상태로 새 자동/수동 시작 및 늦은 레벨 로드 콜백을 차단합니다. 게임 종료 후 이벤트 액터의 직접 `StartEvent` 호출도 거절합니다. 일반 `Stop Event Scheduling`과 달리 **진행 중인 이벤트까지 끝내고 같은 매니저의 재실행을 허용하지 않습니다**. 각 이벤트의 종료 연출은 유지되며, 이미 떨어진 보상 유물을 일괄 삭제하지는 않습니다.

## 검증

`NoPhotos.MapEvents.Manager.CountAndDelay` 자동화 테스트에 고정 첫 시작, 사전 목록 조회, 중복 ID 제외, 후보 부족 시 개수 축소, 이미 실행한 종류 재선택 방지, 실제 종료에 따른 예상 시각 갱신, 무기한 이벤트, 실제 FinishMainGame 경로의 자동/수동/레벨 배치 이벤트 종료, 지연 콜백 및 재실행 차단을 포함합니다. 이번 수정에서는 빌드·자동화 실행·멀티플레이 복제 실행 검증을 하지 않았습니다.
