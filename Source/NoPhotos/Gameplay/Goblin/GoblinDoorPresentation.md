# 고블린 문 등장·퇴장 연출

## 기본 사용

C++ 빌드 후 `BP_GoblinCharacter`의 Class Defaults → `Goblin | Presentation | Door`를 확인합니다.

- `Use Door Presentation`: 기본 true. 문 방식일 때 기존 `On Spawn Presentation Started`와 `On Despawn Presentation Started` 이벤트를 호출하지 않습니다. 해당 이벤트에 연결된 파티클 그래프는 삭제하지 않아도 중복 실행되지 않습니다.
- `Presentation Door Class`: 기본 `NPGoblinPresentationDoor`. 별도 BP 없이 프로젝트의 `/Game/LevelPrototyping/Interactable/Door/Meshes/SM_Door` 문짝과 임시 문틀/검은 내부를 사용합니다.
- `Door Travel Distance`: 기본 250cm. 등장 시 걸어나올 거리와 퇴장 문 위치를 탐색할 거리입니다.
- `Door Walk Speed`: 기본 200cm/s(2m/s). 기존 속도 기반 AnimBP가 걷기/달리기 애니메이션을 담당합니다.
- `Door Presentation Timeout`: 기본 8초. 문 열기, 이동, 닫기 전체 제한 시간입니다.

문 방식은 기존 등장·퇴장 몽타주나 `Finish Spawn/Despawn Presentation` 호출 없이 자동 진행합니다. 이동 애니메이션은 속도 기반 AnimBP를 사용하며, 별도 애니메이션 에셋을 새로 만들지는 않았습니다. 출입 시 루트 모션으로 이동을 중복 적용하지 않도록 합니다.

## 동작

등장: 문 생성·열기 → 고블린 표시 및 실제 NavMesh 이동 → 문 닫기 → 순찰 시작.

퇴장: 가까운 출입 가능한 위치에 문 생성·열기 → 고블린이 검은 내부 뒤쪽으로 이동 → 고블린 숨김 → 문 닫기 → 고블린 제거.

- 문 출입 중에는 AI의 일반 순찰/도주 판단과 사진 판정을 중지합니다.
- 문은 고블린에 붙이지 않는 독립 복제 액터입니다. 고블린이 먼저 제거되어도 닫기 후 자체 제거됩니다.
- 문 위치는 바닥 중앙, +X는 바깥 방향입니다. 문은 장식용으로 충돌과 NavMesh 영향을 끕니다.
- 문 회전은 서버의 시작 시각을 복제하고 각 클라이언트에서 보간합니다. 고블린 이동/숨김은 서버가 결정합니다.
- 문틀을 놓을 공간, 고블린 캡슐 여유 공간, 연결된 NavMesh 경로가 필요합니다. 위치를 찾지 못하면 경고를 남기고 등장/퇴장을 즉시 완료합니다. 이동 실패/시간 초과도 문을 정리하고 종료합니다.
- 등장 전에 이벤트가 종료되면 고블린을 갑자기 드러내지 않고 기존 문만 닫습니다.

`Use Door Presentation`을 끄면 기존 BP 파티클/몽타주 이벤트 및 기존 타임아웃 방식으로 돌아갑니다.

## 문 외형 변경

1. `NPGoblinPresentationDoor`를 부모로 `BP_GoblinDoor`를 만듭니다.
2. Components의 `DoorMesh`에 원하는 문 에셋을 지정합니다. 기본은 얇은 XY축을 문 앞쪽에 맞추고 폭/높이를 자동 조정합니다.
3. `Opening Width`, `Opening Height`, `Frame Thickness`, `Open Angle`, `Open Duration`, `Close Duration`을 조절합니다.
4. 피벗/형태가 특이한 문은 `Auto Fit Door Mesh`를 끄고 `Hinge` 아래 `DoorMesh`의 상대 위치·회전·스케일을 직접 맞춥니다. `Hinge`는 자동으로 문 왼쪽 아래에 배치됩니다.
5. `Interior`는 검은 내부입니다. 기본 임시 엔진 머티리얼 대신 프로젝트용 검은 Unlit 머티리얼로 교체할 수 있습니다. 문틀 3개 컴포넌트의 재질도 변경할 수 있습니다.
6. 고블린의 `Presentation Door Class`를 이 BP로 지정합니다. 문 BP는 레벨에 미리 배치할 필요 없습니다.

문 BP의 Actor Scale은 1을 유지하고 Opening Width/Height로 크기를 조절합니다. 배치 여유 공간 검사는 이 치수를 기준으로 합니다.

## 확인 항목 (빌드/PIE에서 직접 확인 필요)

- 맵 이벤트로 생성: `[GoblinDoor] SPAWN`, 문을 나오기 전 숨김, 이동 완료 후 순찰.
- HP 0 또는 이벤트 종료: `[GoblinDoor] DESPAWN`, 문으로 걸어들어감, 문과 고블린 모두 정리.
- 등장 직후 이벤트 강제 종료: 문만 닫히고 고블린이 갑자기 나타나지 않음.
- 좁은 통로/NavMesh 없음/플레이어가 출입구를 막음: 경고 또는 제한 시간 후 상태가 종료됨.
- 2인 PIE: 양쪽에서 문 열기·닫기와 고블린 표시/숨김 확인.
- `Use Door Presentation=false`: 기존 파티클 이벤트가 다시 실행되는지 확인.

레벨에 직접 놓은 고블린은 기본 Active 상태라 즉시 순찰합니다. 등장 연출 테스트는 기존 고블린 맵 이벤트를 통해 생성해 진행합니다.
