# 고블린 맵 이벤트

## 실행 흐름

1. 이벤트 시작 → 기존 Collector에 등록된 Goblin SpawnVolume에서 랜덤 위치 탐색 → 고블린 한 마리 등장.
2. 현재 로드된 레벨 인스턴스들을 포함해, `RouteGroup == GoblinSpawnGroup`인 유효한 `NPGoblinPatrolRoute` 중 하나를 무작위 선택합니다.
3. 선택한 루트는 해당 고블린이 퇴장할 때까지 고정됩니다. 기존 플레이어 회피/사진 도주는 유지하며, 도주 후에도 같은 루트로 복귀합니다.
4. 사진 HP 0 → 기존 문 퇴장 연출 → 고블린 액터 제거 → `RespawnDelay`(기본 1초) → 볼륨과 루트를 다시 무작위로 선택해 새 고블린 등장.
5. 이벤트 종료 → 재소환/재시도 타이머 취소 → 남아 있는 고블린 퇴장. 퇴장 완료 시점에 이벤트가 끝났다면 재소환하지 않습니다.

등장 중·활동 중·퇴장 중 고블린을 합쳐 항상 최대 한 마리입니다. 이전 BP의 `GoblinCount` 값은 호환을 위해 보존하지만 실행에는 사용하지 않습니다. 무작위 재선택이므로 이전과 같은 볼륨/루트가 다시 선택될 수 있습니다. 볼륨의 기존 Selection Weight는 유지합니다.

## 레벨/BP 설정

- 기존 `BP_GoldenGoblinMapEvent`, `GoblinClass`, `GoblinSpawnGroup`을 사용합니다. 새 이벤트 BP는 필요하지 않습니다.
- 소환 위치는 항상 기존 SpawnVolume입니다. 루트 점이나 SpawnPoint에서 소환하지 않습니다.
- `BP_GoblinRoute` 여러 개를 레벨 인스턴스에 배치합니다. 각 루트는 `Closed Loop=true`, 점 2개 이상, 길이 0 초과, 이벤트와 동일한 Route Group이어야 합니다.
- 스폰 볼륨 바닥과 루트 사이에 고블린이 통과할 수 있는 연결된 NavMesh를 확보합니다. 루트의 월드 위치에는 레벨 인스턴스의 이동/회전/스케일이 반영됩니다.
- NavMesh Bounds와 NavData는 Persistent Level에서 유지해야 합니다. 동적으로 배치되는 방에 맞는 Runtime Generation 설정도 확인합니다. 코드가 NavMesh를 새로 배치하거나 굽지는 않습니다.
- 이벤트 시간은 기존 고블린 이벤트 정의 에셋에서 설정합니다. 재소환으로 시간이 초기화되지 않습니다.
- 루트/볼륨/NavMesh 준비가 늦으면 기본 2초마다 재시도합니다. 루트가 없을 때 임의 배회로 대체하지 않습니다.
- 사용 중인 루트가 더 이상 유효하지 않으면 해당 고블린도 퇴장 후 재선택합니다.

## 스폰 실패 로그

높은 볼륨의 임의 Z가 바닥 NavMesh에서 멀어지는 문제를 줄이기 위해, NavMesh 수직 탐색 범위가 볼륨 전체 높이를 포함합니다. 바닥·경사·충돌·플레이어 경로 검사는 유지합니다. 고블린 캡슐 크기에 맞춰 최소 스폰 높이와 필요 공간을 보정합니다.

실패 시 `LogNPGoblinMapEvent`에 볼륨 수, 이름, 가중치 및 마지막 탐색의 실패 단계별 횟수가 나옵니다.

- `No NavData`: NavMesh 자체가 없습니다.
- `NavProjection`: 후보 위치 주변에 NavMesh를 찾지 못했습니다.
- `OutsideBounds`: NavMesh 위치가 볼륨 밖입니다.
- `Ground`: 바닥/모서리/경사/높이차/볼륨 내부 검사를 통과하지 못했습니다.
- `Clearance`: 고블린이 차지할 공간에 장애물이 있습니다.
- `PlayerPath`: 현재 플레이어에서 후보 지점까지 완전한 경로가 없습니다.
- `Volumes=0`: Collector 로딩, SpawnGroup, LocationSource, 양수 가중치를 확인합니다.

## 검증

빌드 및 PIE는 실행하지 않았습니다.

추가한 자동화 테스트 `NoPhotos.MapEvents.Goblin.SingleGoblinLifecycle`는 네이티브 액터로 다음을 검사합니다: 유효 루트 선택, 기존 Count 값 무시, 중복 소환 요청, 사진 HP 0 후 퇴장 완료까지 슬롯 유지, 재소환 대기 중 이벤트 종료, 이벤트 종료 후 재소환 금지. 실제 볼륨/NavMesh 이동·네트워크 연출은 PIE 검증이 필요합니다.

PIE에서 루트 3개/볼륨 3개를 두고 여러 번 처치하여 `Actor`, `Route`, 생성 위치가 다시 선택되는지 확인합니다. 등장 중/퇴장 중/재소환 대기 중 이벤트를 끝내도 새 고블린이 생기지 않아야 합니다. 두 클라이언트에서 문 연출과 동시 한 마리 제한을 확인합니다.

## 함께 수정한 로그 오류

- `BP_AimableRelic.uasset`: Git LFS 포인터를 34,043바이트 원본으로 복원하고 SHA256 일치를 확인했습니다.
- 이전 `/Game/NoPhotos/Level/Test/LevelInstance/...` 경로 3개: `DefaultEngine.ini` PackageRedirects로 실제 경로에 연결했습니다. 에디터 재시작 후 방 레벨 인스턴스를 열어 참조와 Map Check를 확인하고 저장합니다. 맵 바이너리는 수정하지 않았습니다.
- Pawn의 미등록 `PlayerCamera` 충돌 응답을 제거했습니다. 기본 `Camera` Ignore는 유지합니다.
- 플레이어는 충돌 없는 미사용 PlayerStart를 우선하며, 막혔거나 시작점이 부족하면 근처 위치를 검사합니다. 주변도 전부 막혔다면 강제 생성하지 않고 실패 로그를 남깁니다.
- 산타 선물 유물은 같은 보상 클래스로 최대 250cm 위까지 재탐색합니다. 모든 위치가 막혔을 때는 실패 로그를 유지합니다.
- GroundWind Niagara를 생성자에서 즉시 로드하지 않고 실제 재생 시점에 로드하도록 변경했습니다. ChaosNiagara 플러그인 사용도 프로젝트에 명시했습니다. 기존 `GroundWindSystem`은 Soft Object Reference로 바뀌었으므로 해당 값을 그래프에서 직접 쓰던 BP는 핀/컴파일 상태를 확인합니다.

에셋 로딩·스폰·Niagara 오류의 런타임 해소 여부는 빌드 후 새 로그로 확인해야 합니다. 부가 프로파일러 DLL과 에디터 아이콘 경고는 이번 변경 대상이 아닙니다.
