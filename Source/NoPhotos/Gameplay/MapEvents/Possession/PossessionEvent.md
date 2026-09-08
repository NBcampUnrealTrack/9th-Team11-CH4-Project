# 빙의 이벤트: 등 뒤 유령, WASD 이동 반전, 진열장 잠금 해제

이벤트 동안 플레이어마다 등 뒤 유령을 표시하고 **전후(W↔S)와 좌우(A↔D) 이동을 모두 반전**하며, **맵의 모든 NPRelicCase 진열장을 임시 개방**합니다. 마우스 시점, 점프, 사용 키는 유지합니다. 피해/공격/AI, 사진 판정, UI는 변경하지 않습니다.

기존 유령 이벤트 설정이 완료되어 있다면 사용자 빌드 및 에디터 재시작 후 이벤트 BP의 **Possession Event → Reverse Horizontal Input**(기본 켜짐)만 확인하세요. 입력 매핑 변경, GE BP 생성, 태그 수동 등록, 캐릭터 BP에 컴포넌트 추가는 필요하지 않습니다.

기존 BP 설정을 보존하기 위해 옵션 이름은 유지합니다. 여기서 Horizontal은 수평면 전체로, 이제 W/S와 A/D 모두를 제어합니다.

## 순찰 고스트 (현재 단계)

- 이벤트 시작 시 서버가 `Roaming Ghost Count`만큼 고스트를 생성합니다. 기본값은 3마리이며 BP Class Defaults에서 조절할 수 있습니다.
- 각 고스트는 `Roaming Ghost Route Group`과 같은 그룹의 `NPGhostPatrolRoute`를 개별적으로 무작위 선택합니다.
- 고스트는 겹침을 줄이기 위해 선택한 Spline의 임의 위치와 임의 방향으로 생성되며, 끝에 닿으면 방향을 바꿔 계속 왕복합니다.
- 위치와 회전은 서버에서 계산하고 Replicate Movement로 클라이언트에 전달합니다. 벽 회피/NavMesh 이동은 사용하지 않습니다.
- 현재 단계에서는 플레이어 감지 범위와 추격 전환을 적용하지 않았습니다. 고스트가 순찰 중 플레이어와 실제 접촉하면 기존 빙의 처리는 그대로 실행됩니다.
- 빙의 시간이 끝나면 플레이어 위치가 아니라 순찰 루트의 임의 위치에서 새 고스트가 다시 시작합니다.
- 고스트 한 마리가 플레이어와 접촉해 빙의 중일 때도 나머지 고스트는 계속 순찰합니다. 플레이어별 빙의 시간이 끝나면 비어 있던 한 자리를 다시 생성합니다.
- 이미 빙의된 플레이어와 다른 순찰 고스트가 접촉해도 그 고스트는 소비되지 않습니다.

### 순찰 루트 배치

1. 고스트 이벤트가 로드되는 레벨에 **NPGhostPatrolRoute**를 배치합니다.
2. Spline 점을 이동해 원하는 경로를 만듭니다. 기본은 열린 Spline이며 최소 두 점이 필요합니다.
3. Route의 **Route Group**과 `BP_PossessionEvent`의 **Roaming Ghost Route Group**을 둘 다 `Possession`으로 맞춥니다.
4. `BP_GhostFollower`의 **Roaming Patrol Speed**로 왕복 속도를 조절합니다. 기본값은 200cm/s입니다.
5. `BP_PossessionEvent`의 **Roaming Ghost Count**로 동시 고스트 수를 설정합니다. 기본값은 3입니다.
6. 같은 그룹의 Route를 여러 개 배치하면 각 고스트가 그중 하나를 무작위로 선택합니다. 같은 Route를 여러 고스트가 함께 사용할 수도 있습니다.

루트가 없거나 점이 부족하면 고스트를 생성하지 않고 `Patrol Spawn Retry Interval`마다 다시 찾습니다. Output Log의 `빙의 유령 순찰 생성 대기` 메시지로 확인할 수 있습니다.

## 구조

- `NPPossessionMapEvent`: 기존 `NPMapEvent` 수명과 DA Duration을 사용합니다. 서버가 순찰 루트를 선택하고, 접촉 후 현재 빙의된 `NPStablePhysicsPawn` 목록을 복제합니다. SpawnVolume과 Collector는 사용하지 않습니다.
- `NPGhostFollowerActor`: 순찰 중에는 서버에서 생성되어 위치가 복제됩니다. 빙의 접촉 후 등 뒤 외형은 각 클라이언트와 Listen Server 화면에만 비복제로 생성합니다.
- `NPPossessionGameplayEffect`: 서버가 대상 ASC에 적용하는 무기한 네이티브 GE입니다. `State.ControlsMirrored` 태그를 부여하고 이벤트가 자신의 효과 핸들만 제거합니다.
- `NPControlReversalComponent`: `NPReplicatedStablePhysicsPawn`에 기본 생성됩니다. ASC 태그를 관찰하고 원본 이동 입력의 수평면 성분(X/Y)을 모두 반전해 이동 물리와 그랩 이동 의도에 함께 전달합니다. 수직 성분과 입력 크기는 유지합니다. 일반 `NPStablePhysicsPawn`에는 ASC/이 컴포넌트가 없어 유령만 표시되며 경고가 나옵니다.
- 유령 위치는 캐릭터의 `GetVisualForwardDirection()`으로 계산한 수평 정면을 사용합니다. 기존 캐릭터의 메시 축 보정을 재사용하므로 캐릭터 코드를 추가 수정할 필요가 없습니다.

```text
유령 위치 = 캐릭터 루트 위치 - 수평 정면 × FollowDistance + 월드 위쪽 × HeightOffset
```

실제 외형은 목표 위치/회전을 부드럽게 따라갑니다. 순간이동처럼 거리가 벌어지면 즉시 재배치합니다. 물리 캐릭터의 PostPhysics 갱신 뒤에 추적하도록 Tick 의존성을 설정합니다.

## 에디터 설정 — 사용자가 진행

빌드와 BP/DA/레벨 에셋 생성·수정은 사용자가 진행합니다.

### 1. 유령 외형 BP

1. 부모 **NPGhostFollowerActor**로 `BP_GhostFollower`를 만듭니다.
2. 상속된 **GhostMesh**에 유령 Static Mesh를 지정합니다. 애니메이션이 필요한 Skeletal Mesh라면 **AnimatedGhostMesh**를 사용하세요. 두 메시를 모두 채울 필요는 없습니다.
3. 모델이 반대 방향을 보면 **VisualRoot** 또는 메시의 상대 회전을 조절합니다. 유령 액터의 정면은 +X입니다. 피벗/크기도 VisualRoot 또는 개별 메시에서 보정합니다.
4. 모든 외형의 **Simulate Physics를 끄고 Collision은 NoCollision**을 유지합니다. 유령의 **Replicates와 Replicate Movement는 모두 끕니다.**
5. `Initial Life Span`은 0으로 유지하고 Tick을 켜둡니다. 별도 BP 이동 Tick/Timeline이나 Attach 처리는 추가하지 않아도 됩니다.

프로젝트에서 다음 Static Mesh 파일이 확인되었습니다. 자동 지정하지 않았으므로 원하는 외형을 골라 사용하세요.

```text
Content/NoPhotos/Resources/Exturnal/PolyUniversalPack/Meshes/People/Seasons_People/
  SM_Man_Ghost_Halloween.uasset
  SM_Boy_Ghost_Halloween.uasset
```

| 유령 BP 설정 | 기본값 | 의미 |
| --- | --- | --- |
| Follow Distance | 150cm | 캐릭터 정면 반대 방향으로 떨어진 거리 |
| Height Offset | 70cm | 캐릭터 루트 기준 위쪽 높이. 메시 피벗에 맞게 조절 |
| Follow Interp Speed | 8 | 위치/회전 추적 속도. 0이면 즉시 등 뒤에 붙음 |
| Snap Distance | 600cm | 목표 위치와 이 거리 이상 벌어지면 즉시 재배치 |
| Ghost Max Opacity | 0.35 | 등장 완료 후 알파. 0은 투명, 1은 최대 불투명도 |
| Ghost Fade In Duration | 0.5초 | 등장 페이드 시간. 0이면 즉시 표시 |
| Ghost Fade Out Duration | 0.5초 | 퇴장 페이드 시간. 0이면 즉시 제거 |
| Ghost Opacity Parameter Name | GhostOpacity | 머티리얼 Scalar Parameter 이름 |

페이드 설정은 유령 BP의 **Class Defaults → Ghost Follower → Fade**에서 조절합니다. 유령용 반투명 머티리얼을 메시의 Material 슬롯에 지정하고, `GhostOpacity` Scalar Parameter를 **Opacity / Opacity Override**에 연결하세요. 기존 투명도 마스크가 있다면 마스크에 GhostOpacity를 곱합니다. Opaque 머티리얼에 이름만 추가해서는 반투명해지지 않습니다.

C++이 BeginPlay에서 GhostMesh, AnimatedGhostMesh 및 같은 액터의 추가 MeshComponent 슬롯에 동적 머티리얼을 준비하고 알파를 갱신합니다. 파라미터가 없는 슬롯은 변경하지 않고 `LogNPGhostFollower` 경고를 출력합니다. 여러 슬롯을 쓰면 각 머티리얼에 파라미터가 필요합니다. 머티리얼 에셋 자체는 코드에서 수정하지 않습니다.

**기존 BP에서 직접 작성한 Create Dynamic Material Instance / Set Scalar Parameter Value / 알파 Timeline 연결은 사용자가 제거하거나 실행선을 끊어 주세요.** C++과 BP가 동시에 알파를 쓰면 연출이 충돌합니다. 기존 BP의 `MaxOpacity` 변수가 아닌 새 네이티브 **Ghost Max Opacity**를 조절합니다. BP에 새 Timeline이나 종료 이벤트를 만들 필요는 없습니다. 다른 BeginPlay 작업은 유지해도 됩니다.

유령은 벽/플레이어를 밀거나 카메라 충돌을 만들지 않으며, 벽을 통과할 수 있는 시각 연출입니다.

### 2. 빙의 이벤트 BP

1. 부모 **NPPossessionMapEvent**로 `BP_PossessionEvent`를 만듭니다.
2. Class Defaults의 **Possession Event → Ghost Class**에 `BP_GhostFollower`를 지정합니다.
3. **Player Refresh Interval**은 기본 0.2초로 두면 됩니다. **Reverse Horizontal Input**은 켜짐이 기본이며, 끄면 이전처럼 유령만 표시합니다.
4. 이벤트 BP의 **Replicates / Always Relevant는 켜고 Replicate Movement는 끕니다.**
5. Apply Event State를 BP에서 재정의하지 않아도 됩니다. 재정의한다면 부모 호출을 유지하세요.

Ghost Class가 비어 있거나 추상 클래스이면 `LogNPPossessionEvent` 경고가 한 번 나오며 유령은 표시하지 않습니다. 입력 반전, 진열장 개방, 이벤트 Duration은 외형 유무와 별개로 유지됩니다.

### 3. 이벤트 DA / 카탈로그

1. **일반 NPMapEventDefinition** 타입의 DA를 만듭니다. 이번에는 별도 Possession 전용 DA 클래스가 없습니다.
2. Event Class에 `BP_PossessionEvent`를 지정하고, Event Id(예: `Possession`), 표시 이름(예: `빙의`), Type, Scale을 설정합니다.
3. Duration을 예를 들어 **30초**로 설정합니다. 0이면 기존 공통 이벤트 규칙대로 수동 종료 전까지 유지됩니다.
4. 기존 `DA_EventCatalog → Event Entries`에 해당 DA와 선택 가중치를 넣습니다.
5. SpawnVolume, Collector, NavMesh는 필요 없습니다. `Possession` 태그는 이미 등록되어 있으므로 새 Gameplay Tag를 만들 필요가 없고, 고스트 이벤트가 로드되는 레벨에는 위의 `NPGhostPatrolRoute`가 있어야 합니다.

메타데이터/기간은 DA, 유령 외형 클래스는 이벤트 BP, 외형·추적 거리·높이는 유령 BP가 관리합니다.

## 진열장 임시 개방

- 별도 맵 이벤트/BP/DA를 만들지 않습니다. 기존 Possession 시작·종료에 자동으로 연결되며 같은 DA Duration을 사용합니다. Duration이 0이면 FinishEvent 또는 이벤트 액터 제거까지 유지됩니다.
- 서버가 현재 월드의 모든 `ANPRelicCase`를 대상으로 처리합니다. 태그/대상 목록을 지정할 필요가 없으며, 진행 중 생성되거나 스트리밍된 케이스는 Player Refresh Interval(기본 0.2초)마다 확인합니다.
- 개방 시 유물 잠금을 해제하고 유물 충돌을 켭니다. 파괴되지 않은 유리 Geometry Collection은 NoCollision으로 전환하므로 유리를 깨지 않고 유물을 집을 수 있습니다. 케이스 액터/받침대 전체를 숨기거나 제거하지 않으며 유물은 잡기 전까지 전시 상태를 유지합니다.
- 일반 열쇠 기믹의 잠금 상태와 이벤트별 임시 해제를 분리합니다. 최종 상태는 `일반 기믹 해제 OR 임시 해제 이벤트가 하나 이상 존재`이며, 기존 `bIsUnlocked`/`IsUnlocked()`는 이 최종 상태를 나타냅니다. 이벤트 중 열쇠로 해금/재잠금한 결과도 이벤트 종료 후 반영합니다.
- 이벤트가 끝나면 해당 이벤트의 해제만 제거합니다. 다른 빙의가 남아 있거나 일반 기믹으로 해금된 케이스는 계속 열려 있습니다. 이벤트를 강제로 Destroy한 경우에도 EndPlay에서 제거합니다.
- 다시 잠기는 경우 아직 `IsDisplayed()`인 유물만 잠금과 충돌 비활성 상태로 되돌립니다. 이벤트 중 한 번이라도 집어 전시 상태를 벗어난 유물 및 반납된 유물은 위치/잡기/충돌 상태를 변경하지 않습니다. 이미 파괴된 케이스도 복구하거나 재잠그지 않습니다.
- 정상 열쇠 경로의 LockCase에도 같은 유물 재잠금 정책을 적용합니다. UnlockCase/LockCase의 반환값은 일반 기믹 상태가 변경되었는지를 뜻하므로, 빙의 중 UnlockCase가 성공해도 최종 개방 상태는 이미 true일 수 있습니다.
- 최종 케이스 상태와 슬롯 상태는 기존 복제로 전달합니다. 클라이언트가 임의로 잠금을 변경하지 않습니다. 초기 상태가 BeginPlay보다 먼저 복제되면 케이스 초기화 후 적용합니다.
- 열림/닫힘 외형은 기존 `OnCaseUnlocked` / `OnCaseLocked` BP 이벤트를 재사용합니다. 유리 표시나 이동 애니메이션을 원하면 이 이벤트에서 처리하세요. 별도 이벤트 연결은 필요 없고, BP에서 C++ 잠금 상태나 유리 충돌을 반대로 덮어쓰지 않도록 확인합니다.
- Reverse Horizontal Input을 꺼도 진열장 개방은 유지됩니다. 유령 표시/입력 반전과 독립된 빙의 효과입니다.

## 생성 / 정리 규칙

- 이벤트가 진행 중이면 중도 참가 및 리스폰한 새 Pawn도 다음 목록 갱신에 반영합니다.
- 관전 Pawn이나 AI만 조종하는 Pawn은 대상에서 제외합니다. 현재 PlayerController가 조종하는 `NPStablePhysicsPawn` 계열만 포함합니다.
- 대상당 로컬 유령 하나만 유지합니다. Pawn 복제가 늦거나 아직 BeginPlay 전이면 다음 갱신에서 다시 확인합니다.
- 조종 해제/접속 종료로 목록에서 빠지거나 대상 캐릭터가 제거되면 `RequestFadeOut()`으로 유령 퇴장을 시작합니다. 추적을 중단하고 마지막 위치에서 현재 알파부터 0으로 페이드한 뒤 스스로 파괴합니다.
- 이벤트 종료/이벤트 액터 Destroy 시 갱신 타이머와 GAS 효과는 즉시 정리하고 유령 외형만 페이드 시간 동안 남습니다. 입력 반전 해제는 페이드를 기다리지 않습니다. 빠르게 재시작하면 새 유령이 등장하는 동안 이전 유령의 퇴장 연출이 잠시 겹칠 수 있습니다.
- 레벨/PIE 종료 시에는 퇴장 대기 없이 정리합니다. 생성 실패한 유령도 즉시 파괴합니다. 퇴장 요청은 중복 호출해도 타이머를 다시 시작하지 않으며 Tick이 꺼진 경우에도 LifeSpan으로 잔여 유령을 제거합니다.
- Dedicated Server에서는 대상 목록과 GAS 효과를 관리하고 유령 외형을 생성하지 않습니다.
- 유령은 충돌 없는 로컬 연출이며 서버 사진 판정용 대상이 아닙니다. 실제 촬영 이미지에 보이는지/어떻게 취급할지는 별도 사진 기능 정책이며 이번 작업에서는 수정하지 않았습니다.
- 조종 해제/대상 변경/이벤트 종료/액터 제거 시 이 이벤트가 적용한 GE를 제거합니다. 중도 접속/리스폰/ASC 초기화 지연은 기존 대상 갱신에서 재시도합니다.
- 여러 빙의 효과가 겹쳐도 태그 개수가 1 이상이면 한 번만 반전합니다. 다른 이벤트의 효과가 남아 있으면 반전을 유지하고 마지막 효과가 사라질 때 복구합니다. 이벤트가 켜진 동안 외부에서 GE만 제거하면 다음 갱신에 재적용됩니다.

## 입력과 네트워크 처리

- 기존 입력/RPC 형식을 유지하여 원본 월드 이동 벡터와 카메라 Yaw를 함께 보냅니다. 전후/좌우 전체 반전 계산은 Yaw와 무관하게 X/Y 부호를 바꾸며, 별도 시점 회전 RPC 도착 순서에 의존하지 않습니다.
- 소유 클라이언트와 서버는 각각 자신의 GAS 태그 상태로 원본 입력을 한 번 변환합니다. 반전한 결과를 다시 서버에 보내지 않습니다.
- 원본 입력을 보관하여 키를 누른 채 효과가 시작/종료돼도 상태 변경 시 재계산합니다. 정지 RPC에서도 원본 캐시를 비워 이후 효과 변경이 이전 이동을 되살리지 않게 합니다.
- 기존 물리 권한 구조(평상시 소유 클라이언트 기준 Root, 공유 Grab 시 서버 기준)는 유지합니다. 이번 변경은 별도 치트 방지나 네트워크 예측 재설계가 아닙니다. 태그 복제 지연 동안 상태 차이는 발생할 수 있으므로 지연 환경 테스트가 필요합니다.
- 이동 RPC 인자가 변경되므로 **클라이언트와 서버를 같은 소스로 빌드**해야 합니다.

## 검증

요청에 따라 빌드/PIE/자동화 테스트는 실행하지 않습니다. `NPPossessionEventTests.cpp`에 유령 위치, 페이드 알파, 왕복 거리, WASD 반전 수학 테스트를 작성했습니다. 빌드 후 `NoPhotos.MapEvents.Possession` 필터에서 `GhostFollowTransform`, `GhostFadeOpacity`, `GhostPatrolPingPong`, `HorizontalInputReversal`을 실행할 수 있습니다. 자동화 소스는 계산을 검사하며 실제 머티리얼 표시/액터 수명/GAS 수명/복제 동작은 아래 수동 확인이 필요합니다.

수동 확인 항목(아직 실행하지 않음):

1. 이벤트 시작 시 본인과 다른 플레이어마다 유령 하나가 표시되는지.
2. 캐릭터 회전/이동/점프/순간이동 시 뒤쪽으로 따라오는지. 모델 방향은 VisualRoot로 조절합니다.
3. Listen Server 및 Dedicated Server + 클라이언트 2개에서 중복 생성 없이 보이는지.
4. 이벤트 중 접속/리스폰/접속 종료/관전 전환 시 대상이 갱신되는지.
5. 종료/재시작/레벨 종료 시 잔여 유령이 남지 않는지.
6. W/S와 A/D가 모두 뒤바뀌며 점프·마우스·사용 키가 유지되는지. 카메라를 90도/180도 돌린 뒤와 대각선 이동에서도 확인합니다.
7. W/S 또는 A/D를 누른 채 이벤트 시작/종료 시 즉시 반전/복구되는지. 키를 놓은 뒤 이벤트 상태가 바뀌어도 다시 움직이지 않는지.
8. 빙의 이벤트 두 개를 겹쳐 시작한 뒤 하나만 종료하면 반전을 유지하고, 둘 다 종료하면 정상으로 돌아오는지.
9. Listen Server 호스트 및 원격 클라이언트, Dedicated Server 클라이언트에서 동일한 규칙인지. 네트워크 지연/패킷 손실과 공유 Grab 전후도 확인합니다.
10. 유령 자체는 충돌을 만들지 않는지. Reverse Horizontal Input을 끈 이벤트는 이동을 반전하지 않고 유령 표시와 진열장 개방을 유지하는지.
11. 등장 시 0 → Ghost Max Opacity, 종료 시 현재 알파 → 0으로 부드럽게 변하는지. 등장 도중 종료해도 알파가 튀지 않는지.
12. Fade In/Out Duration을 각각 0으로 설정한 경우, 다중 머티리얼 슬롯, Static/Skeletal Mesh에서 표시가 맞는지.
13. 이벤트 액터 Destroy/대상 Pawn 제거/퇴장 중 재시작 시 잔여 유령이 페이드 후 사라지는지. 퇴장 요청을 반복하거나 퇴장 도중 Tick을 꺼도 LifeSpan 이후 제거되는지.
14. 빙의 시작 시 모든 진열장이 개방되고 유리 충돌 없이 유물을 집을 수 있는지. 종료 시 남아 있는 전시 유물은 다시 잠기며 이미 집은 유물은 이동/충돌 상태를 유지하는지.
15. 시작 전에 열쇠로 열린 케이스, 진행 중 열쇠로 해금/재잠금한 케이스가 종료 후 해당 기믹 상태를 따르는지.
16. 빙의 두 개 중 하나만 종료해도 진열장은 계속 개방되고 마지막 이벤트 종료 시 복구되는지. 이벤트 강제 Destroy 및 반복 시작/종료도 확인합니다.
17. 기존에 파괴된 케이스는 복구되지 않는지. 빙의 중 생성/스트리밍된 케이스와 중도 접속 클라이언트에도 개방 상태가 반영되는지.
18. Listen Server 및 Dedicated Server에서 케이스 외형/유리 충돌/유물 잠금이 일치하는지. 종료 직전 잡기와 네트워크 지연 환경도 확인합니다.
