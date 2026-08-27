# 투명화 영역의 시야 제한 — 에디터 설정과 연결점

## 적용 방식

서버의 기존 SpawnVolume 판정 → `UNPInvisibilityGameplayEffect` → `State.VisionRestricted` 복제 → Pawn의 `VisionRestriction` 컴포넌트 → 조작 중인 로컬 카메라에 후처리 안개.

영역 밖에서는 효과가 없고, 영역 안에서만 안개가 적용됩니다. 영역 이탈/이벤트 종료/기존 Pawn 종료 때 자신이 추가한 CameraModifier만 제거합니다. 겹친 볼륨, 중도 참가, 리스폰은 기존 이벤트의 GE 수명 관리를 따릅니다. 원격 Pawn과 Dedicated Server는 상태만 처리합니다. 관전/별도 CameraActor를 ViewTarget으로 쓰는 화면에는 적용하지 않습니다.

**코드만 반영하면 안개가 보이지 않습니다. 아래 머티리얼을 사용자가 생성·지정해야 합니다.** 블루프린트 그래프 작업이나 맵의 ExponentialHeightFog 추가는 필요하지 않습니다.

## 1. 안개 머티리얼 만들기

기존 캐릭터용 `CharInvisible`과는 **별도** 머티리얼입니다. 예: `M_VisionRestrictionFog`.

머티리얼 빈 공간을 클릭하고 Details에서 설정합니다.

| 설정 | 값 |
|---|---|
| Material Domain | Post Process |
| Blendable Location | Scene Color After Tonemapping |
| Is Blendable | 켜기 |

이 머티리얼을 맵 PostProcessVolume이나 카메라의 Post Process Materials에 직접 넣지 않습니다. 그러면 이벤트 밖에서도 적용되거나 중복될 수 있습니다. 아래 컴포넌트에만 지정합니다.

### 필요한 노드

우클릭 검색으로 추가합니다. Scalar Parameter는 `S`를 누른 채 빈 곳을 클릭해도 됩니다.

| 노드 | 설정 |
|---|---|
| SceneTexture | 노드를 선택하고 Scene Texture Id = PostProcessInput0 |
| SceneDepth | 입력 핀은 연결하지 않음. 현재 픽셀의 전방 깊이 |
| Scalar Parameter | 이름 `FogStartDistance`, 기본값 `300` |
| Scalar Parameter | 이름 `FogEndDistance`, 기본값 `1000` |
| Vector Parameter | 이름 `FogColor`, RGB 기본값 `(0.12, 0.14, 0.16)` |
| Subtract | 2개 |
| Max | 1개, B = `1` |
| Divide | 1개 |
| Saturate | 1개 |
| LinearInterpolate (Lerp) | 1개 |
| ComponentMask | 2개, 각각 RGB만 체크 |

연결 순서:

1. 첫 번째 Subtract: A = `SceneDepth`, B = `FogStartDistance`.
2. 두 번째 Subtract: A = `FogEndDistance`, B = `FogStartDistance`.
3. Max: A = 두 번째 Subtract 결과, B = `1`.
4. Divide: A = 첫 번째 Subtract 결과, B = Max 결과.
5. Divide → Saturate → Lerp의 Alpha.
6. `SceneTexture:PostProcessInput0`의 Color → RGB ComponentMask → Lerp의 A.
7. `FogColor` → RGB ComponentMask → Lerp의 B.
8. Lerp → 머티리얼 출력의 **Emissive Color**. 저장/적용합니다.

식으로는 다음과 같습니다. Custom 노드를 추가하라는 뜻이 아니라 위 노드 연결의 요약입니다.

```text
FogAlpha = saturate((SceneDepth - FogStartDistance) / max(FogEndDistance - FogStartDistance, 1))
OutputRGB = lerp(PostProcessInput0.rgb, FogColor.rgb, FogAlpha)
```

기본값 기준 깊이 300cm에서는 원래 색, 650cm에서는 원래 색과 안개색의 중간, 1000cm 이상에서는 안개색입니다. 가까운 물체는 보이고 먼 배경이 가려집니다. 머티리얼 미리보기 구 대신 실제 플레이 화면에서 확인해야 합니다.

파라미터 이름은 코드와 정확히 같아야 합니다. 런타임에는 코드가 이 세 값을 덮어쓰므로 튜닝은 아래 Pawn 컴포넌트에서 합니다. [Epic 후처리 머티리얼 문서](https://dev.epicgames.com/documentation/unreal-engine/post-process-materials-in-unreal-engine?application_version=5.7)도 참고할 수 있습니다.

## 2. 캐릭터 BP에서 지정하기

1. 사용자가 C++를 빌드하고 에디터를 다시 열어 기본 컴포넌트 추가를 반영합니다.
2. 실제 플레이에 사용하는 `BP_RepPawn`을 엽니다.
3. 상속된 **VisionRestriction** 컴포넌트를 선택합니다.
4. `Vision Restriction | Fog`의 **Vision Fog Material**에 위 머티리얼을 지정합니다.
5. **Fog Settings**에서 다음 값을 조절합니다.

| 항목 | 기본값 | 의미 |
|---|---|---|
| Fog Start Distance | 300cm | 안개가 시작되는 전방 깊이 |
| Max View Distance | 1000cm | 안개가 완전히 덮는 전방 깊이 |
| Fog Color | 어두운 회청색 | 먼 배경의 색 |

Max View Distance는 Fog Start Distance보다 크게 설정합니다. 코드도 시작값을 0 이상, 최대값을 시작값+1cm 이상으로 보정합니다. 서버와 클라이언트는 동일한 Pawn BP 기본값을 사용합니다. 런타임 설정 변경/복제 기능은 이번 범위에 없습니다.

기존 투명화 이벤트 BP, DA, Collector/Volume 그룹 설정은 유지합니다. 안개 머티리얼은 **이벤트 BP가 아닌 Pawn의 VisionRestriction**에 지정합니다.

## 3. 팀원 연결 계약 — 아직 사진/UI에는 연결하지 않음

Pawn에서 `UNPVisionRestrictionComponent`를 찾아 사용합니다.

| API | 의미 |
|---|---|
| IsVisionRestricted() | 현재 시야 제한 상태 |
| GetMaxViewDistance() | 현재 최대 전방 깊이(cm). 비활성이면 **0 = 제한 없음** |
| GetVisionRestrictionSettings() | 보정된 시작/최대 거리와 색. 활성 여부와 무관하게 설정 반환 |
| OnVisionRestrictionChanged(bool) | 상태 진입/해제 알림. 최초 연결 시 상태도 직접 조회 |

서버에서는 촬영자의 ASC `NPGameplayTags::State_VisionRestricted` 또는 위 컴포넌트를 조회합니다. 클라이언트 화면의 머티리얼 유무는 서버 상태를 바꾸지 않습니다. 머티리얼 미설정이어도 태그와 거리 API는 활성 상태를 반환합니다.

**거리 기준은 구형 반경이 아니라 SceneDepth와 같은 카메라 전방 깊이입니다.** 화면 가장자리는 카메라와의 직선거리가 더 길 수 있습니다. 서버 담당자가 같은 경계를 적용하려면 검증한 촬영 원점과 전방 벡터를 기준으로 계산합니다.

```text
ViewDepth = Dot(TargetPosition - ValidatedCameraLocation, ValidatedCameraForward)
```

제한 활성 중 `ViewDepth >= MaxViewDistance`인 지점은 완전 안개 구간입니다. 뒤쪽 물체, 프러스텀, 가려짐, 대상 바운드와 증거 인정 조건 등은 기존 사진 판정과 함께 담당자가 처리해야 합니다. 이 문서는 판정 기준을 전달할 뿐 사진 검증 코드를 수정하지 않습니다.

사진 SceneCapture는 플레이어 CameraModifier를 자동 상속하지 않습니다. 사진 담당자가 같은 후처리 머티리얼과 컴포넌트 설정을 별도 Capture에 적용하고, 후처리를 포함하는 CaptureSource를 선택해야 합니다. 사진 카메라의 위치/FOV가 다르면 같은 공간에서도 결과가 달라질 수 있습니다.

촬영 대상이 투명한지는 **대상의** `State.Invisible`, 촬영자의 시야가 제한되는지는 **촬영자의** `State.VisionRestricted`로 구분합니다. 현재 GE는 둘 다 부여하지만 별도 효과가 `State.Invisible`만 부여하면 안개는 생기지 않습니다.

## 4. 렌더링 범위와 수동 확인

이 효과는 깊이 버퍼 기반의 화면 안개입니다. 물리적인 볼륨 안개나 보안상의 가시성 판정이 아닙니다. 깊이를 기록하지 않는 반투명 캐릭터/유리/파티클은 뒤쪽 불투명 물체의 깊이를 따라 안개가 섞일 수 있습니다. 따라서 가까운 자기 반투명 캐릭터도 배경에 따라 더 흐려질 수 있습니다. 투명화에서 유물 메시를 숨기지는 않지만 유물도 화면의 안개 영향을 받을 수 있습니다. 반투명 효과별 별도 깊이/패스 대응은 추가 렌더링 작업입니다.

1. A만 영역 안, B는 밖: A 화면만 안개. 양쪽 투명화 외형도 기존대로 적용.
2. A의 카메라 앞 3m, 6.5m, 10m에 불투명 테스트 물체 배치: 원래색/중간/안개색 확인. 깊이는 Pawn 중심이 아닌 카메라 기준.
3. 진입/이탈 반복, 영역 안에서 이벤트 종료, 겹친 볼륨에서 하나만 이탈: 안개 잔류/깜박임 확인.
4. 중도 참가, 리스폰, 빙의 해제, 별도 ViewTarget으로 전환 후 복귀: 대상 카메라만 적용되는지 확인.
5. Listen Server 호스트/원격 클라이언트, Dedicated Server 구성에서 각각 확인.
6. 기존 카메라 후처리와 사진 줌 FOV가 유지되고, 이벤트 종료 시 원래 화면으로 복원되는지 확인.
7. 원경/하늘/유리/파티클/자기 반투명 메시/유물 및 사용 중인 AA·해상도 스케일에서 실제 렌더링 확인.
8. 머티리얼을 비우거나 파라미터 이름을 틀리게 하면 `LogNPVisionRestriction` 경고가 Pawn당 한 번 나오고, 투명화와 서버 상태는 유지되는지 확인.

빌드/PIE/에셋 수정은 수행하지 않았습니다. 소스와 엔진 API의 정적 검토만 진행했습니다.
