# 스포트라이트 맵 이벤트

## 에디터 설정

1. `NPEventSpotlight`를 부모로 조명 BP를 만든다.
   - `Spotlight` 컴포넌트에서 밝기, 색, `Attenuation Radius`, 안쪽/바깥쪽 원뿔 각도를 설정한다.
   - 기본 조사 방향은 아래쪽이다. `Sweep Angle`은 좌우 최대 기울기(기본 25도), `Sweep Period`는 왕복 시간(기본 8초)이다.
   - 필요하면 조명 외형 메시를 BP에 추가한다. 위치와 조사 방향의 기준은 스폰 포인트 Transform이다.
2. `NPSpotlightMapEvent`를 부모로 이벤트 BP를 만들고 `Spotlight Class`에 조명 BP를 지정한다.
   - `Location Source`: Point
   - `Spotlight Spawn Group`: Spotlight
   - `Light Duration`: 10초 / `Dark Duration`: 2초
   - `Bonus Interval`: 2초 / `Bonus Rate`: 0.1
3. 맵 또는 이벤트 위치 레벨에 `NPMapEventLocationCollector`와 `NPMapEventSpawnPoint`들을 배치한다.
   - 포인트는 바닥이 아니라 **실제 조명이 생성될 높이**에 배치한다.
   - Collector와 포인트는 같은 레벨에 둔다.
   - Collector의 `Default Spawn Groups`에 Spotlight를 넣고, 포인트의 `Use Collector Spawn Groups`를 켠다. 개별 포인트에 Spotlight 그룹을 직접 지정해도 된다.
   - 포인트의 `Enabled`를 켜고 `Selection Weight`는 0보다 크게 둔다. 가중치는 기존 Collector의 사용 가능 조건이며, 조명 배치 개수를 추첨하는 용도가 아니다.
   - 포인트마다 조명 하나를 생성한다. 4~5개 제한이 없고, 포인트를 추가·삭제하면 생성 개수도 따라 바뀐다.
   - 기본 회전에서는 포인트 로컬 Y 방향으로 빛이 좌우로 움직인다. 포인트 Yaw로 좌우 이동 방향을 맞춘다.
4. `NPMapEventDefinition` 데이터 에셋을 만든다.
   - `Event Class`: 이벤트 BP
   - `Event Id`: 다른 이벤트와 겹치지 않는 ID(예: Spotlight)
   - 이름과 설명, **전체 이벤트의 Duration과 종료 후 Delay**를 설정한다.
   - 10초 점등과 2초 소등을 반복하려면 전체 Duration을 충분히 길게 설정한다. 예를 들어 60초로 설정할 수 있다. Definition 기본값 8초는 첫 점등 도중 이벤트를 종료시킨다.
5. 현재 사용하는 `NPMapEventCatalog`의 `Event Entries`에 정의와 추첨 가중치를 추가한다.
   - 위치 전용 레벨을 사용하면 `Location Level Instance`와 `Location Level Transform`도 지정한다.
   - 이벤트별 알림 UI를 사용하면 해당 UI 데이터 테이블에도 동일한 Event Id 행을 추가한다.

## 동작

- 이벤트 시작 시 메인 Persistent Level에 직접 배치된 `ARectLight`를 모두 수집하여 현재 밝기에서 5 Candelas로 부드럽게 낮추고, 전체 이벤트 종료 시 160 Candelas로 부드럽게 복원한다. 이벤트 BP의 `Spotlight Event → Lighting → Rect Light Fade Duration`에서 전환 시간(기본 1초, 0이면 즉시)을 설정한다. 조명의 2초 소등 구간에도 5 Candelas를 유지한다.
- 밝기 복원은 이벤트가 종료된 뒤에도 완료될 때까지 진행한다. 액터 자체가 제거되는 EndPlay에서는 Tick을 계속할 수 없으므로 160 Candelas로 즉시 복원한다.
- 이 밝기 변경은 서버와 각 클라이언트에서 적용한다. RectLight의 Mobility는 런타임 밝기 변경이 가능한 Stationary 또는 Movable로 설정해야 한다. 액터 이름은 사용하지 않으므로 메인 레벨에 RectLight를 추가하면 함께 적용된다.
- 모든 지정 포인트에서 조명을 생성한 뒤 그중 한 개만 균등 추첨해 켠다. 직전에 켜졌던 조명이 다시 선택될 수 있다.
- 조명의 위치는 고정하며, 조사 방향만 서버 시간 기준으로 회전한다.
- 10초 점등 후 2초간 모두 끄고 다시 한 개를 선택한다. 이 2초는 Definition의 이벤트 후딜레이와 별개다.
- 서버는 매 프레임 캐릭터의 실제 오른손 그랩 대상과 조사 범위를 확인한다.
- 조사 범위는 Unreal의 `USpotLightComponent::AffectsBounds`로 물리 Pawn 루트의 Bounds와 원뿔·거리 범위의 교차를 확인한다. 실제 메시 표면 단위의 판정은 아니므로 경계에 Bounds만 걸쳐도 대상이 될 수 있다.
- 조명에서 Pawn Bounds 중심까지 Visibility 채널을 막는 장애물이 있으면 제외한다. 자기 캐릭터와 잡고 있는 유물은 이 차폐 검사에서 제외한다.
- 같은 유물을 잡고 범위 안에서 연속 2초를 채울 때마다 기본 가격의 10%를 누적한다. 범위 이탈, 그랩 해제·대상 교체, 소등 시 체류 기록을 초기화한다.
- 같은 유물을 여러 명이 잡아도 유물별 2초 보상을 중복 지급하지 않는다.
- 증가한 금액은 이벤트 종료 후에도 유지되며, 사진 감점과 별도로 계산한다. 가격 UI와 실제 반환 점수 모두 `GetCurrentPrice()`를 사용한다.
- 전체 이벤트 종료와 EndPlay에서 조명과 그랩 델리게이트를 정리한다. 생성 가능한 포인트가 없으면 다음 Tick에서 이벤트를 종료한다.

## 직접 확인할 항목

- 메인 레벨 RectLight들이 이벤트 시작 시 5 Candelas, 종료 시 160 Candelas가 되는지 서버와 클라이언트에서 확인한다.
- 포인트 1개와 여러 개에서 생성 수가 일치하고, 동시에 한 개만 켜지는지 확인한다.
- 10초 점등 / 2초 소등과 좌우 회전을 확인한다.
- 유물 기본 가격 1,000일 때 유효 체류 2초마다 100씩 증가하는지 확인한다.
- 2초 전에 이탈하거나 놓았다 다시 잡으면 시간이 초기화되는지 확인한다.
- 두 플레이어가 같은 유물을 잡아도 증가량이 두 배가 되지 않는지 확인한다.
- 사진 감점이 있는 유물의 UI 금액과 반환 점수가 일치하는지 확인한다.
- 리슨 서버와 클라이언트, 이벤트 도중 접속, 전체 이벤트 종료와 게임 종료를 확인한다.

코드 정적 점검만 수행했으며, 프로젝트 빌드와 PIE 검증은 수행하지 않았다.
