# 산타 선물 투하 설정

## 동작

1. 서버가 산타의 **현재 비행 경로 위치**에서 선물 액터를 생성합니다. X/Y는 경로 그대로이며 기본 100cm 아래에서 투하합니다.
2. 선물은 수평 속도를 받지 않고 중력으로 수직 낙하합니다. 서버의 ProjectileMovement가 Box Collision을 Sweep하며, 클라이언트는 서버 위치를 복제받습니다.
3. 바닥에 닿으면 낙하만 멈추고 **닫힌 상태로 플레이어의 잡기를 기다립니다.** 착지만으로 개봉/유물 생성/상자 제거 타이머가 시작되지 않습니다.
4. 플레이어가 기존 손 잡기로 상자를 처음 잡으면 서버가 개봉을 시작합니다. 잡기 등록 다음 틱에 손을 자동 해제하고 착지 위치에서 기존 뚜껑 연출을 재생합니다. 기본 1초 뒤 DA 후보 중 **유물 하나를 균등 추첨하여 한 번만 생성**하고 `ANPBaseRelic::ReleaseWithVelocityImpulse`로 물리를 켭니다. 계속 잡고 있을 필요는 없습니다.
5. 열린 상자는 기본 3초 후 제거합니다. 유물은 선물/산타 이벤트와 무관하게 월드에 남습니다.

산타가 떠나거나 이벤트를 수동 종료하면 이후 투하만 취소합니다. 이미 투하한 선물은 착지 후 잡기를 기다리며 이벤트 종료 후에도 잡아서 개봉할 수 있습니다. 개봉 중인 선물은 연출과 유물 생성을 마칩니다. 선물과 유물은 Persistent Level에 생성하므로 서버 전용 경로 레벨의 언로드에도 제거되지 않습니다. 미개봉 상자에는 별도 자동 제거 시간이 없습니다.

## 1. 선물 BP 만들기 — 사용자가 진행

부모 클래스 **NPSantaGiftActor**로 `BP_SantaGift`를 생성하세요. 배치용 액터가 아니라 이벤트가 런타임에 생성할 클래스입니다. 새 C++ 클래스가 반영된 빌드가 먼저 필요합니다.

### 외형 컴포넌트

| 컴포넌트 | 설정 |
| --- | --- |
| `CollisionBox` | 상자 전체를 감싸는 Box Extent. 기본 반크기 (35,35,35)cm. 루트 중심 기준으로 크기 조절 |
| `VisualRoot` | 상자 메시 전체의 상대 위치/크기 보정. 상자 바닥이 CollisionBox 바닥에 맞게 조절 |
| `ClosedBoxMesh` | 낙하 중 사용할 닫힌 상자 Static Mesh |
| `OpenBoxMesh` | 뚜껑이 없는 상자 본체 Static Mesh |
| `LidPivot` | 뚜껑의 회전 기준. 상대 위치로 축/힌지를 정함 |
| `LidMesh` | 분리된 뚜껑 Static Mesh. LidPivot 아래에서 위치를 맞춤 |

프로젝트에 아래 파일이 존재하는 것을 확인했습니다. 실제 크기/피벗/색상 일치는 에디터에서 조절하세요. 메시를 직접 수정하지는 않았습니다.

```text
Content/NoPhotos/Resources/Exturnal/PolyUniversalPack/Meshes/Christmas/
  SM_Gift_Christmas_Cube_A.uasset              (닫힌 상자 후보)
  SM_Gift_Christmas_Cube_Open_Base.uasset      (본체)
  SM_Gift_Christmas_Cube_Open_Lid.uasset       (뚜껑)
```

- `OpenBoxMesh`와 `LidMesh`를 **둘 다** 지정하면 뚜껑 개봉 연출을 사용합니다. 두 파트만으로 닫힌 외형을 구성해 `ClosedBoxMesh`를 비워도 됩니다.
- 닫힌 일체형 메시만 지정하면 상자가 살짝 커졌다가 작아져 사라지는 대체 연출을 사용합니다. **실제 뚜껑이 열리는 연출에는 분리 본체/뚜껑 메시가 필요합니다.**
- 모든 시각 메시의 **Simulate Physics는 끄고 Collision은 NoCollision**을 유지하세요. 충돌 판정은 루트 `CollisionBox`만 담당합니다.
- `CollisionBox`의 Object Type은 PhysicsBody를 유지하세요. 낙하 중 서버는 Query Only, 클라이언트는 NoCollision이며 착지 대기 중에는 양쪽 모두 Query And Physics로 바뀝니다. 물리 시뮬레이션 없이 위치를 고정합니다. WorldStatic·WorldDynamic은 Block, Pawn·PhysicsBody 등은 Ignore이므로 단순 접촉으로 개봉하지 않습니다. 개봉 시 잡기를 해제하고 NoCollision으로 전환합니다.
- `Replicates`, `Always Relevant`, **Replicate Movement 모두 켜기**. 산타 비행 BP는 Replicate Movement를 끄지만 **선물 BP는 켭니다.**
- `FallingMovement`의 중력/최대 속도는 조절할 수 있습니다. 기본 중력 배율 1, 최대 속도 2000cm/s입니다. 수평 Velocity, Bounce, Homing, 물리 시뮬레이션은 추가하지 마세요.
- `Initial Life Span`은 0을 유지합니다. C++가 낙하 타임아웃과 개봉 후 수명을 관리합니다.

### 개봉 / 유물 조절

| BP 설정 | 기본값 | 의미 |
| --- | --- | --- |
| Opening Duration | 1초 | 첫 잡기로 개봉을 시작한 뒤 개봉 완료 및 유물 생성까지 시간 |
| Lid Lift Height | 60cm | 뚜껑이 올라가는 높이(VisualRoot 좌표계) |
| Lid Open Rotation | Pitch -110도 | 초기 뚜껑 회전에서 더할 개봉 회전 |
| Opened Life Span | 3초 | 개봉 완료 후 상자 잔해 유지 시간 |
| Relic Spawn Height | 100cm | 착지한 상자 루트 중심에서 유물 생성 높이 |
| Relic Pop Up Speed | 150cm/s | 생성된 유물이 위로 살짝 튀는 속도. 0도 가능 |
| Max Fall Duration | 45초 | 바닥을 못 찾은 선물의 최대 낙하 시간 |
| Minimum Ground Normal Z | 0.5 | 바닥으로 인정할 표면의 위쪽 방향 기준 |

`On Gift Landed`, `On Gift Opened`는 소리/파티클 등을 추가할 수 있는 **연출 전용 BP 이벤트**입니다. 기본 뚜껑 애니메이션은 C++에 있으므로 BP Tick/Timeline은 필요 없습니다. 여기에서 유물 Spawn을 추가하면 중복될 수 있으므로 넣지 마세요. 전용 서버에서는 연출 훅을 실행하지 않습니다.

`GrabbableComponent`는 C++에서 기본 생성되므로 BP에 중복 추가하거나 별도 잡기 이벤트를 연결하지 마세요. 기존 BP의 `On Gift Landed`에 직접 개봉 Timeline이나 Destroy를 연결했다면 제거하고 착지 소리/먼지 효과만 유지하세요. 낙하 중이나 개봉이 시작된 상자는 잡을 수 없으며 동시/반복 잡기도 한 번의 개봉으로 처리합니다.

## 2. 기존 DA_Santa에 투하 설정 추가

DA 타입은 기존 **NPSantaEventDefinition** 그대로 사용합니다.

| DA 설정 | 권장 시작값 |
| --- | --- |
| Gift Class | `BP_SantaGift` |
| Gift Drops → Count | 5 |
| Gift Drops → Start Progress | 0.1 |
| Gift Drops → End Progress | 0.9 |
| Gift Drop Height Offset | 100cm |
| Relic Classes | 생성할 기존 유물 BP 여러 개 |

- `Count=5`, 구간 `0.1~0.9`이면 **매 비행마다** 진행률 **10%, 30%, 50%, 70%, 90%**에 투하합니다. `Flight Schedule → Flight Duration`이 30초라면 각 비행 시작 후 약 3, 9, 15, 21, 27초입니다. 공통 `Duration`은 대기 시간을 포함한 전체 이벤트 수명이며 투하 간격 계산에는 사용하지 않습니다.
- 비행 종료 후 `Respawn Delay Min~Max` 사이 랜덤 대기를 거쳐 새 경로/방향으로 다시 등장하면 투하 슬롯도 0번부터 다시 시작합니다. 이전 비행에서 떨어진 선물은 유지합니다. 역방향 비행도 산타가 이동하는 방향 순서대로 투하합니다.
- 마지막 비행 도중 전체 이벤트 시간이 만료되면 아직 실행하지 않은 투하 슬롯은 취소됩니다. 비행 시간을 줄여 남은 선물을 몰아서 떨어뜨리지 않습니다.
- Count=1이면 Start Progress에서 한 개, Count=0이면 비행만 합니다. 최대 128개입니다.
- End Progress는 1보다 작아야 합니다. 비행 종료 타이머와 같은 시각의 투하는 허용하지 않습니다.
- 서버가 크게 지연되면 과거 슬롯을 한 위치에 몰아서 생성하지 않고 건너뜁니다. 그래서 심한 지연 상황에서는 실제 투하 수가 Count보다 적을 수 있습니다.
- 유물 목록은 **ANPBaseRelic 파생 BP 클래스**입니다. 메시나 데이터 테이블 Row를 넣는 칸이 아닙니다. 기존 유물 BP가 가진 가격/등급/상호작용 설정을 그대로 사용합니다.
- 후보는 중복 제거 후 균등 추첨합니다. 서로 다른 상자에서 같은 유물이 나올 수 있습니다. 한 종류만 넣으면 해당 유물만 나옵니다.
- 랜덤 풀을 비우거나 Gift Class를 누락하면 경고를 남기고 **비행만** 진행합니다. 유물 전체 에셋을 임의로 스캔하지 않습니다.
- 이번 작업에서 BP/DA/레벨 에셋은 수정하거나 생성하지 않았습니다.

## 착지 / 실패 조건

- 바닥/지붕처럼 위쪽을 향한 충돌면이면 착지합니다. 지형만 구별하는 태그나 전용 착지 볼륨은 사용하지 않습니다. **실내 바닥 대신 건물 지붕에 떨어질 수 있으므로 경로를 확인하세요.**
- 바닥은 WorldStatic 또는 WorldDynamic이고 PhysicsBody에 Block 응답이 있어야 합니다. NavMesh/Collector/SpawnVolume은 필요 없습니다.
- 벽면 충돌, 초기 관통, 바닥 없는 낙하 타임아웃에서는 경고를 남기고 선물을 제거하며 유물을 생성하지 않습니다.
- 유물은 충돌 위치를 조정해 스폰을 시도합니다. 그래도 막혀 있으면 관통 생성하지 않고 실패를 기록합니다. 큰 유물은 `Relic Spawn Height`를 높이고 여유 공간을 확보하세요.
- 유물 BP는 물리용 Simple Collision과 Query And Physics 설정이 필요합니다. `ReleaseWithVelocityImpulse`가 실패하면 로그를 남깁니다. 파괴형/특수 기믹 유물은 단독 런타임 생성과 물리 해제가 가능한지 확인 후 후보에 넣으세요.
- 착지 여부, 개봉 시작 여부/서버 시각, 착지 위치를 함께 복제합니다. 착지 후 오래 기다린 상자에 중도 접속해도 닫힌 상태이며, 개봉 중 접속하면 잡기로 시작된 현재 진행률을 표시합니다. 클라이언트는 개봉 확정/랜덤 추첨/유물 생성을 수행하지 않습니다.

## 확인 절차 — 아직 실행하지 않음

사용자 제한에 따라 빌드, PIE, 자동화 테스트 실행 없이 소스 정적 검사만 진행합니다. 기존 `NPSantaFlightTests.cpp`에 `NoPhotos.MapEvents.Santa.GiftSchedule` 계산 테스트를 추가했습니다.

1. 30초/5개 설정에서 경로를 따라 떨어지는지, Count=0/1도 정상인지.
2. 서로 다른 유물 BP 두 개 이상을 넣고 반복 실행했을 때 후보 안에서 랜덤 생성되는지.
3. 착지 후 Opening Duration/Opened Life Span보다 오래 기다려도 닫힌 상태로 남는지. 첫 잡기 후에만 개봉하고 서버 유물 하나가 생기는지.
4. Listen/Dedicated Server + 클라이언트 2개에서 낙하·개봉·유물 수가 일치하는지.
5. 낙하 중/착지 대기 중/개봉 중 접속해 위치와 개봉 상태가 맞는지. 오래 대기해도 잡기 시 연출이 0부터 시작하는지.
6. 마지막 상자가 떨어지는 중 이벤트가 끝나도 착지하고, 이벤트 종료 후 잡아서 개봉할 수 있으며 유물이 남는지.
7. 이벤트 재시작 시 이전 투하 타이머가 남지 않는지. 이전에 떨어진 선물이 아직 남아 있는 것은 의도한 동작입니다.
8. 벽/바닥 없음/충돌하는 유물 생성 위치에서 실패 로그가 나오고 중복 보상이 생기지 않는지.
9. 두 플레이어가 동시에 잡거나 잡기를 반복해도 한 번만 개봉하는지. 잡기 직후 손이 자동 해제되고 상자에 붙잡히지 않는지.
10. 낙하 중에는 잡히지 않고, 착지한 상자는 Listen Server 호스트와 원격 클라이언트 모두 잡아서 개봉할 수 있는지.
