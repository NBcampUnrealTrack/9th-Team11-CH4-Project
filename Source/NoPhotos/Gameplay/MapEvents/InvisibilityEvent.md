# 투명화 이벤트 — SpawnVolume 영역 적용

## 범위

- 이벤트가 활성화된 동안 서버가 지정 SpawnVolume 안의 플레이어 Pawn에만 `UNPInvisibilityGameplayEffect`를 적용합니다. 모든 적용 영역에서 나가면 해당 효과를 제거합니다.
- GE가 `State.Invisible`과 `State.VisionRestricted`를 부여합니다. 외형과 시야 컴포넌트는 각각의 태그를 관찰합니다.
- 투명화된 캐릭터는 조작 중인 자기 화면에서 기본 불투명도 `0.15`, 다른 플레이어의 화면에서는 메시 비표시로 처리합니다. 영역 밖 캐릭터는 평상시 외형입니다.
- `ANPReplicatedStablePhysicsPawn`에 `Invisibility`와 `VisionRestriction` 컴포넌트를 기본 서브오브젝트로 추가했습니다.
- 캐릭터 소유 Skeletal/Static Mesh만 변경합니다. 별도 Actor인 유물, 자식 Actor, Widget Component는 변경하지 않습니다.
- 캐릭터 그림자는 투명화 상태 동안 끄고 복원합니다. 충돌, 물리, 애니메이션, 이동, 잡기는 변경하지 않습니다.
- 영역 안의 로컬 플레이어 카메라에만 거리 기반 안개를 추가합니다. 기본 전방 3m부터 짙어지고 10m부터 안개색으로 덮습니다. FOV나 맵 전체 안개는 변경하지 않습니다.
- 이름표, 사진 이미지, 사진 증거 판정은 변경하지 않습니다. 안개용 후처리 머티리얼은 사용자가 만들고 지정해야 합니다. [안개 설정 방법](VisionRestrictionFog.md)을 참고합니다.
- 투명화 이벤트 중 대상 박스에 가장자리와 모서리가 부드럽게 사라지는 깊이 기반 안개를 표시할 수 있습니다. SpawnVolume 자체는 수정하지 않고 이벤트가 표시 메시를 소유합니다. 전용 Surface 머티리얼의 Custom 노드 설정은 [부드러운 영역 안개](SoftRegionFog.md)를 참고합니다.

## 사용자가 에디터에서 설정할 내용

### 이벤트 정의와 카탈로그

1. 사용자가 C++ 빌드 후 에디터를 다시 열어 새 클래스와 기본 컴포넌트를 반영합니다.
2. 기존 `NPMapEventDefinition` 형식의 DA를 생성합니다.
3. `EventClass`를 `NPInvisibilityMapEvent`, `EventId`를 고유한 값(예: `Invisibility`)으로 설정합니다.
4. `DisplayName`, `Description`, `EventType`, `EventScale`, `Duration`을 DA에서 설정합니다.
5. 기존 카탈로그의 `EventEntries`에 이 정의와 선택 가중치를 추가합니다. 메인 맵의 영역을 사용하면 위치 레벨은 비워 두고, 이벤트 전용 영역 레벨을 사용하면 `LocationLevelInstance`와 필요시 `LocationLevelTransform`을 지정합니다.

이번 클래스는 공통 메타데이터를 C++에 중복 선언하지 않습니다. 기존 Definition DA를 사용하며, 이벤트 BP 없이 네이티브 클래스를 직접 지정할 수 있습니다. `Colossal`은 기존 매니저의 일반 추첨에서 제외되므로 일반 이벤트 테스트에는 사용하지 않습니다.

### SpawnVolume 배치와 그룹

기존 반투명 머티리얼/플레이어 컴포넌트 설정은 그대로 사용합니다. 아래 영역 설정만 추가합니다.

1. 메인 맵 또는 카탈로그에 연결할 이벤트 위치 레벨에 `NPMapEventSpawnVolume`을 배치하고 박스 크기/위치/회전을 정합니다.
2. 볼륨과 **같은 레벨**에 `NPMapEventLocationCollector`가 있어야 합니다. 이미 있다면 재사용할 수 있습니다.
3. 볼륨의 `SupportedSpawnGroups`에 `Invisibility`를 추가합니다. 이 태그는 `DefaultGameplayTags.ini`에 등록했습니다.
4. 또는 Collector의 `DefaultSpawnGroups`에 `Invisibility`를 추가하고 볼륨의 `Use Collector Spawn Groups`를 켜도 됩니다. 그러면 그 Collector의 그룹을 상속하는 모든 볼륨이 대상입니다.
5. 볼륨의 `SelectionWeight`는 0보다 커야 합니다. 양수인 볼륨은 모두 적용 영역이며, 이번 이벤트에서는 확률 추첨하지 않습니다.

기존 이벤트 BP를 사용한다면 `Location Source`를 `Volume`으로 확인합니다. `Both`도 볼륨을 수집하지만 `Point`는 사용할 수 없습니다. `Invisibility Event | Volumes`의 설정은 다음과 같습니다.

| 항목 | 기본값 | 역할 |
|---|---|---|
| `InvisibilitySpawnGroup` | `Invisibility` | 적용할 SpawnVolume의 Gameplay Tag 그룹 |
| `VolumeCheckInterval` | `0.1`초 | 서버의 영역 진입/이탈 확인 주기 |

그룹을 바꾸면 볼륨/Collector도 같은 그룹으로 맞춥니다. `State.Invisible`은 플레이어의 상태 태그이므로 영역 그룹에 넣는 태그와 다릅니다.

판정 기준은 서버의 `Pawn->GetActorLocation()`이 박스 안에 있는지입니다. 손끝만 들어간 경우에는 적용되지 않을 수 있으며, 박스 높이도 캐릭터 기준 위치를 포함하도록 배치해야 합니다. 박스의 회전과 스케일을 반영하고 경계는 내부로 취급합니다. 충돌/Overlap/NavMesh 설정은 필요하지 않습니다.

Collector에 수집된 영역을 매 판정마다 조회하므로 등록된 레벨의 언로드나 볼륨 파괴/이동/그룹 변경도 다음 판정에 반영됩니다. **런타임에 새 볼륨을 생성했다면** 해당 Collector의 `RefreshLocations(Volume 또는 Both)`를 호출해 수집 목록을 갱신해야 합니다. 기존 매니저는 이벤트 시작과 위치 레벨 로드 완료 때 수집을 수행합니다.

설정된 볼륨이 없으면 아무에게도 적용하지 않으며 `LogNPInvisibilityEvent` 경고를 이벤트당 한 번 출력합니다. 전체 플레이어 적용으로 되돌아가지 않습니다.

### 자기 캐릭터 반투명 머티리얼

캐릭터 BP에 상속된 `Invisibility` 컴포넌트의 `Invisibility | Appearance`에서 설정합니다.

| 항목 | 기본값 | 역할 |
|---|---|---|
| `SelfOpacity` | `0.15` | 자기 캐릭터 불투명도. 0은 투명, 1은 불투명 |
| `OpacityParameterName` | `InvisibilityOpacity` | 머티리얼의 전역 Scalar Parameter 이름 |
| `SelfMaterialOverride` | 미지정 | 지정 시 자기 캐릭터의 모든 대상 메시 슬롯에 사용할 머티리얼 |

빠른 확인용 머티리얼은 사용자가 `Surface / Translucent`로 만들고 `InvisibilityOpacity` Scalar Parameter를 `Opacity`에 연결합니다. Skeletal Mesh에서 사용할 수 있도록 머티리얼 사용 설정도 확인합니다. 이를 `SelfMaterialOverride`에 지정하면 이벤트 동안만 동적 인스턴스로 교체합니다. 모든 슬롯에 같은 머티리얼을 쓰므로 원래 의상/피부 외형을 자동으로 보존하지는 않습니다.

원래 외형을 유지하려면 각 기존 머티리얼이 투명화를 지원하도록 사용자가 준비하고 `SelfMaterialOverride`를 비워 둡니다. `Translucent`의 Opacity 또는 `Masked`의 디더 마스크에 해당 파라미터를 연결합니다. 단순히 Scalar를 Masked의 Opacity Mask에 바로 연결하면 반투명이 아닌 임계값 기반 표시/비표시가 됩니다.

**머티리얼 준비 없이 Opaque 머티리얼의 알파만 변경할 수는 없습니다.** 지원하지 않는 슬롯은 원본을 유지하고 `LogNPInvisibility` 경고를 출력합니다. 따라서 머티리얼 미설정 상태에서는 자기 캐릭터가 불투명하게 남을 수 있습니다. 다른 플레이어의 메시 비표시는 이 머티리얼 설정과 무관합니다.

## 수명과 복제

- 이벤트의 지속시간은 기존 Definition DA와 `ANPMapEvent`가 관리합니다.
- GE는 `Infinite`, 중첩 방식은 `None`입니다. 각 적용을 독립 핸들로 보관합니다.
- 이벤트 시작 시 즉시 판정하고, 이후 기본 0.1초 간격으로 영역 안의 플레이어 ASC만 확인합니다. 중도 참가/리스폰/초기화 지연 후 새 ASC도 영역 안에 있을 때만 적용합니다. 화면 반영에는 네트워크 복제 지연이 추가됩니다.
- 여러 적용 볼륨이 겹쳐도 이 이벤트의 GE는 플레이어당 하나만 유지합니다. 볼륨 하나를 나가도 다른 적용 볼륨 안이면 유지합니다.
- 적용 영역에서 나가거나 해당 영역이 없어지면 다음 판정에서 이 이벤트의 GE를 제거합니다. 이벤트 전체는 계속 활성화되므로 재진입하면 다시 적용합니다.
- 빙의 해제된 이전 Pawn의 효과는 다음 갱신에서 제거합니다.
- 이벤트 종료와 `EndPlay`에서 **이 이벤트가 부여한 핸들만** 제거합니다.
- 다른 이벤트/유물이 같은 상태 태그를 유지하면 외형도 계속 투명화 상태를 유지합니다.
- 이번 단계에는 해제/면역 정책이 없습니다. 이벤트가 활성화되고 플레이어가 영역 안에 있는 동안 외부에서 이 이벤트의 GE를 지우면 다음 갱신에서 다시 적용합니다.
- 기존 ASC의 Mixed 복제를 사용합니다. 다른 플레이어의 전체 GE Spec을 읽거나 표현용 Multicast에 의존하지 않습니다.
- 외형 컴포넌트는 시작 시 현재 태그도 조회합니다. 투명화 중에만 0.1초 간격으로 로컬 조작 여부와 새 메시를 확인합니다.
- 시야 컴포넌트도 시작 시 현재 태그를 조회합니다. 제한 중에만 0.1초 간격으로 로컬 카메라를 확인하고, 이 컴포넌트 전용 CameraModifier를 추가/제거합니다. 기존 카메라의 후처리 설정과 블렌드 가중치는 덮어쓰지 않습니다.
- 영역 이탈/이벤트 종료 시 자신의 GE가 부여한 두 태그가 함께 해제됩니다. 다른 출처가 `State.VisionRestricted`를 유지하면 안개도 유지합니다. `State.Invisible`만 부여하는 별도 효과에는 안개가 자동으로 붙지 않습니다.
- Dedicated Server는 상태와 알림만 처리하며 머티리얼과 메시를 변경하지 않습니다. Listen Server는 호스트 화면의 외형도 처리합니다.
- 종료 시 기존 숨김/그림자 상태와 교체 전 머티리얼을 복원합니다. 다른 시스템이 이미 교체한 머티리얼 슬롯은 덮어쓰지 않습니다.
- 클라이언트당 로컬 플레이어 1명을 기준으로 합니다. 분할 화면의 관찰자별 표현은 별도 작업입니다.

## 팀원 연결점

### 권위 있는 게임 상태

서버 사진 판정에서는 대상 ASC의 `NPGameplayTags::State_Invisible` 보유 여부를 조회하면 됩니다. 자기 화면에서 반투명하게 보이는 것은 로컬 표현일 뿐, 서버의 투명화 상태는 동일합니다. 머티리얼 알파나 `HiddenInGame`을 게임 규칙 판정에 사용하지 않습니다.

```cpp
const bool bInvisible = TargetASC && TargetASC->HasMatchingGameplayTag(NPGameplayTags::State_Invisible);
```

`TargetASC`는 기존 AbilitySystemInterface/AbilitySystemBlueprintLibrary로 얻습니다. 관련 헤더는 `AbilitySystemComponent.h`, `Core/GameplayTag/NPGameplayTags.h`입니다.

### UI 등 상태 구독

Pawn에서 `UNPInvisibilityComponent`를 찾아 다음 기능을 사용합니다.

- `IsInvisible()`: 현재 상태. 최초 연결 시에도 조회합니다.
- `OnInvisibilityChanged(bool bInvisible)`: 상태 시작/종료 알림. 서버와 해당 Pawn이 존재하는 각 클라이언트에서 발생합니다.

이름표 담당자는 기존 표시 조건에 이 상태를 추가하면 됩니다. 현재 이름표는 매 Tick 자신의 표시 여부를 갱신하므로 외부에서 한 번 숨기는 방식만으로는 유지되지 않습니다.

사진 증거 제외, 사진 SceneCapture의 가시거리 표현, 서버 촬영 거리 제한은 담당자의 후속 작업입니다. `UNPVisionRestrictionComponent`의 `IsVisionRestricted()`, `GetMaxViewDistance()`, `GetVisionRestrictionSettings()`, `OnVisionRestrictionChanged`를 연결점으로 제공합니다. **사진 SceneCapture가 플레이어 카메라의 CameraModifier를 자동으로 받지는 않습니다.** 깊이 기준과 담당자 연결 계약은 [안개 설정 방법](VisionRestrictionFog.md)에 정리했습니다.

## 사용자 확인 목록 — 아직 실행하지 않음

1. 플레이어 A는 영역 안, B는 밖에 둔 상태에서 이벤트를 시작합니다. A 화면에서는 A가 반투명/B가 평상시 외형이고, B 화면에서는 A가 안 보이고/B는 평상시 외형인지 확인합니다.
2. A가 나가면 양쪽 화면에서 A가 복원되고, B가 들어가면 B에게만 투명화가 적용되는지 확인합니다. 재진입도 확인합니다.
3. 두 볼륨을 겹쳐 배치하여 한 볼륨만 나갔을 때는 유지되고, 모든 볼륨 밖으로 나갔을 때 해제되는지 확인합니다.
4. 회전/스케일된 박스, 서로 다른 높이, 볼륨 그룹/가중치 0, Collector 그룹 상속을 확인합니다. 적용 볼륨이 없는 경우에는 누구도 투명해지면 안 됩니다.
5. 영역 내부에서 이벤트가 종료되거나 이벤트 액터가 제거돼도 원래 머티리얼/숨김/그림자로 복원되는지 확인합니다.
6. 영역 안/밖에서 각각 중도 참가·리스폰하고 새 Pawn이 올바르게 적용되는지 확인합니다. 볼륨을 파괴하거나 위치 레벨을 언로드해도 효과가 남지 않는지 확인합니다.
7. Listen Server 호스트도 같은 결과인지 확인합니다. Dedicated Server 구성이 있다면 별도로 확인합니다.
8. 투명화 중 유물이 보이고 이동/충돌/잡기가 유지되는지 확인합니다. 이름표/사진은 이번 변경으로 연동됐다고 간주하지 않습니다.
9. 다른 출처의 투명화 효과를 추가한 뒤 영역에서 나갔을 때 그 효과까지 제거하지 않는지 확인합니다.
10. 기존 머티리얼 설정과 종료 후 복원, 이벤트 재실행도 확인합니다.
11. 안개 머티리얼 설정 후 A만 영역 안에 있을 때 A 화면만 제한되는지 확인합니다. 이탈/종료/리스폰 후 안개가 남지 않고, 기존 사진 줌 FOV와 카메라 효과가 유지되는지 확인합니다.

빌드, PIE, 블루프린트/머티리얼/DA 에셋 생성 및 수정은 수행하지 않았습니다. 소스 정적 검토만 진행했습니다.
