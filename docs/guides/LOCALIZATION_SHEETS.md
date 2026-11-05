# 시트로 String Table 번역하기

이 안내는 기존 Unreal String Table 에셋의 원문과 번역을 CSV 시트에서 검토하고 Unreal의 현지화 파이프라인에 반영하는 절차다. 사용자는 원문 한국어, 번역 영어(2026-11-04 추가), 중국어 간체·번체, 일본어, 이탈리아어, 프랑스어, 브라질과 포르투갈의 포르투갈어를 선택했다. 프로젝트 Game 현지화 대상과 패키징에 해당 문화가 설정되어 있고 동일 설정의 문화 코드는 격리 UE 5.8.4에서 확인했다. 실제 프로젝트의 수집 범위·패키지 표시는 검증이 남았다. 시트는 개인 Google 계정에 생성했고 팀 공유 위치·담당자는 미정이다. 아래 `en`은 설정된 문화 중 하나를 예로 든 것이고, 경로와 테이블 이름은 명령 설명용 예시다. 다른 문화는 `--culture`를 바꿔 같은 순서로 진행한다.

## 범위와 준비

- 대상은 **기존 String Table 에셋 하나**다. `ST_Session`은 예시이며 에셋 경로를 Editor에서 확인한다. `table`은 원문 CSV를 만들고, `po`는 해당 테이블의 번역을 기존 PO에 채운다. 스크립트는 에셋을 직접 수정하거나 현지화 결과를 컴파일하지 않는다.
- C++의 `LOCTEXT` 및 다른 에셋의 텍스트는 Unreal의 Gather 대상이 될 수 있지만 이 시트 도구가 자동으로 수집·변환하지 않는다. PO의 다른 항목은 보존한다. 음성·이미지 등 문화별 에셋 교체도 이 절차의 범위 밖이다.
- 같은 `.uasset`과 Localization Target은 한 번에 한 담당자만 수정한다. 다른 Editor의 미저장 상태와 패키지 소유·잠금·인계를 확인하고, 시작 전 에셋과 기존 현지화 파일을 백업한다. 시트 편집자는 기준 CSV와 사용한 에셋 개정, 시트 사본, 변경 시점을 공유하고 서로 덮어쓰지 않는다.
- 모든 산출물의 출력 경로는 새 파일이어야 한다. 실패하거나 내용이 달라지면 원인을 확인한 뒤 새 이름으로 다시 생성한다. 되돌릴 수 있도록 기준 export와 Gather/Export 직후 PO를 별도로 보관한다.

## 순서

1. **기준 추출.** UE 5.8.4 Editor에서 프로젝트와 Python Script Plugin을 열고 Output Log의 명령 콘솔에서 다음을 실행한다. 출력 폴더는 미리 만들고 에셋 경로와 절대 출력 경로는 실제 값으로 바꾼다. 기준 CSV와 함께 로그의 **Table ID와 namespace**를 기록한다. 이 스크립트는 읽기 전용 export만 하며 namespace는 CSV에 들어가지 않는다.

   ```text
   py "D:/DIGETHER/scripts/localization/unreal_string_table.py" "/Game/Frontend/Localization/ST_Session" "D:/LocalizationWork/baseline.csv"
   ```

   기준 export 직후 PowerShell에서 `Get-FileHash DIGETHER/Content/Frontend/Localization/ST_Session.uasset -Algorithm SHA256`을 실행해 해시와 에셋 경로를 기록한다. 해시는 저장된 파일만 확인하므로 에디터에 미저장 변경이 없는지도 별도로 확인해야 한다.

2. **시트 시작 파일 생성.** 저장소 루트 PowerShell에서 다음을 실행한다. `--culture`는 대상마다 반복할 수 있다. `--table`은 시트 행을 선택하는 식별자로 작업 내내 동일하게 쓴다.

   ```powershell
   python scripts/localization/sheet_pipeline.py bootstrap --baseline D:/LocalizationWork/baseline.csv --table ST_Session --culture en --output D:/LocalizationWork/starter.csv
   ```

3. **시트 작성.** 승인된 시트 서비스에 starter CSV를 가져와 `Table`, `Key`, `SourceString`, `POContext`, `Translation:en`, `ReviewedSource:en` 열을 유지한다. `POContext`는 처음에는 비어 있다. 번역자가 원문을 검토하고 번역을 작성한 후 검토한 원문을 해당 문화의 `ReviewedSource:en`에 그대로 기록한다. 원문이 바뀌면 번역을 다시 검토한 뒤에만 이 값을 갱신한다. UTF-8 CSV로 내보내고 기준에 있던 키는 모두 남긴다. 새 키는 추가할 수 있지만 키 삭제·이름 변경은 참조와 번역 식별자에 영향을 주므로 별도 검토한다. `{PlayerName}`, `{0}` 같은 FText 변수와 표기법을 그대로 보존한다.

4. **원문 테이블 생성과 Import.** 에셋의 미저장 변경과 잠금·담당 상태를 확인하고, 현재 에셋을 별도 새 CSV로 다시 내보낸다. 시트 CSV와 기준·현재 export를 지정하면 도구가 두 export의 키·원문·메타데이터를 비교한 뒤 완전한 테이블 CSV를 생성한다. 생성 결과의 diff를 검토한다. 이 단계는 번역을 Import하지 않는다. 패키지 파일의 해시도 기준 추출 시점과 비교한다. 어느 쪽이든 달라졌거나 에디터의 미저장 상태를 확인할 수 없으면 Import를 중단하고 최신 에셋을 기준으로 차이를 조정한다. 도구는 현재 export가 실제로 최신인지 또는 검사 이후 에셋이 변경됐는지 확인할 수 없으므로 Import 직전까지 잠금·담당 상태를 유지한다. 변경이 없을 때 수정 전 백업을 확인하고 String Table Editor에서 CSV를 Import한 뒤 키·원문·기존 메타데이터를 확인하고 저장한다.

   ```powershell
   python scripts/localization/sheet_pipeline.py table --sheet D:/LocalizationWork/sheet.csv --baseline D:/LocalizationWork/baseline.csv --current D:/LocalizationWork/current.csv --table ST_Session --output D:/LocalizationWork/table-import.csv
   ```

5. **Gather와 PO Export.** Localization Dashboard의 해당 Target에서 승인된 native culture, 대상 cultures, 소스 코드·에셋 Gather 경로 및 로딩 정책을 확인하고 Gather Text 후 대상 문화의 PO를 Export한다. Gather 범위나 문화가 미정이면 임의로 확정하지 않는다. Export한 PO와 에셋 namespace를 보관한다. Unreal이 생성하는 manifest와 archive는 직접 편집하지 않는다.

6. **POContext 확인 및 번역 반영.** Export된 PO에서 각 행의 실제 `msgctxt`와 `msgid`를 확인해 `POContext`에 정확히 입력한다. 이 값은 추측하거나 Table ID로 대체하지 않는다. 각 행은 선택한 테이블의 실제 namespace와 Key에 대응하고 `msgid`가 시트의 `SourceString`과 정확히 같아야 한다. 키·원문이 바뀌었으면 새 기준과 PO로 다시 확인한다. 시트를 다시 CSV로 내보낸 뒤, 로그에서 확인한 실제 namespace를 `--namespace`에 넣어 문화별로 실행한다.

   ```powershell
   python scripts/localization/sheet_pipeline.py po --sheet D:/LocalizationWork/sheet-with-context.csv --input-po D:/LocalizationWork/Game-en-export.po --table ST_Session --culture en --namespace "여기에 실제 namespace" --output D:/LocalizationWork/Game-en-translated.po
   ```

   스크립트는 PO의 Language, namespace·키·원문 일치, 빈 번역, 변수 이름 집합을 확인한다. 해당 문화의 `ReviewedSource`가 현재 `SourceString`과 다르면 PO 생성이 거절된다. 중복·불일치, fuzzy 또는 복수형 항목, 지원하지 않는 PO 구문이나 변수 표기는 오류로 멈추므로 원본과 로그를 확인해 수동 검토한다. 변수 검사 통과는 문법 전체와 번역 품질을 보증하지 않는다. 생성된 PO의 변경 내용과 다른 항목 보존 여부도 검토한다.

7. **Import·Compile·표시 검증.** 검토된 PO를 해당 Target과 문화에 Import하고 Compile Text를 실행한다. 설정된 문화가 패키징에 포함되는지 확인하고 실제 패키지에서 해당 문구, 변수 치환, 줄바꿈·글꼴·잘림 및 원문 fallback을 확인한다. 검증 환경, 문화, 결과와 미검증 항목을 기록한다. CSV 생성 성공만으로 Editor Import나 게임 표시 성공을 뜻하지 않는다.

## 도구 한계

기준 CSV는 비어 있으면 안 되며 기존 키가 시트에서 빠지거나 기준·현재 export가 다르면 `table`은 중단한다. 두 인수에 같은 파일을 지정할 수 없다. 기존 메타데이터의 모호한 역슬래시나 Unreal CSV 이스케이프가 왕복되지 않는 텍스트도 거부한다. `po`는 선택 테이블의 모든 행에 대해 해당 문화 번역과 고유한 POContext를 요구하고, 지원하는 단순 FText 변수만 검사한다. 수동 예외 처리 시 결과를 임의로 덮어쓰지 말고 별도로 검토한다. 입력 시트와 생성 파일의 접근 권한·백업·승인은 팀 운영으로 관리한다.

## 공식 참고 자료

- [String Tables — UE 5.8](https://dev.epicgames.com/documentation/unreal-engine/using-string-tables-for-text-in-unreal-engine?application_version=5.8)
- [Localization Tools — UE 5.8](https://dev.epicgames.com/documentation/unreal-engine/localization-tools-in-unreal-engine?application_version=5.8)
- [Localization Overview — UE 5.8](https://dev.epicgames.com/documentation/unreal-engine/localization-overview-for-unreal-engine?application_version=5.8)
- [Text Localization — UE 5.8](https://dev.epicgames.com/documentation/unreal-engine/text-localization-in-unreal-engine?application_version=5.8)
- [Scripting the Unreal Editor Using Python — UE 5.8](https://dev.epicgames.com/documentation/unreal-engine/scripting-the-unreal-editor-using-python?application_version=5.8)
