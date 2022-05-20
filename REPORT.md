# EduBtM Report

Name: DoHoon Kim

Student id: 20170806

# Problem Analysis



# Design For Problem Solving

## High Level



## Low Level



# Mapping Between Implementation And the Design

- EduBtM_CreateIndex()
    1. `sm_CatOverlayForSysTables` 에서 `sm_CatOverlayForBtree` 의 firstPage 확인
    2. `btm_AllocPage` 를 통해 root page 할당
    3. `edubtm_InitLeaf` 를 통해 root page init
    4. 2에서 읽어 온 페이지 `Bfm_FreeTrain`

- edubtm_InitLeaf()
    
    
- edubtm_InitInternal()
    
    
- EduBtM_DropIndex()
    1. root page에 `edubtm_FreePages()`

- EduBtM_InsertObject()
    1. `edubtm_Insert()`
    2. root page가 split되면, `edubtm_root_insert()`

- edubtm_Insert()
    - root page를 `BfM_GetTrain()`
    - root가 internal page이면,
        - object를 삽입할 child node를 찾은 후, (`edubtm_BinarySearchInternal()`)
        - 해당 subtree에 `edubtm_Insert()`
        - child node에서 split이 발생하면, root에 split된 페이지를 가리키는 internal index entry를 추가
            - root에 삽입할 slot #를 먼저 결정하고, (`edubtm_BinarySearchInternal()`)
            - root에 `edubtm_InsertInternal()`
        
    - root가 leaf page면,
        - `edubtm_InsertLeaf()`
    
    - “root에서” split이 발생하면, split된 페이지(새로 생긴 페이지)를 가리키는 internal index entry 반환
    - root page를 `BfM_SetDirty()`
    - root page를 `BfM_FreeTrain()`

- edubtm_BinarySearchInternal()
    1. lpage의 slot #가 index entry들의 key가 오름차순으로 정렬돼 있다고 가정
    2. low, high, mid를 init
    3. binary search를 수행 `edubtm_KeyCompare()`
    
- edubtm_BinarySearchLeaf()
    
    
- edubtm_InsertLeaf
    1. index entry를 삽입할 slot #를 정한다. `edubtm_BinarySearchLeaf()`
    2. `edubtm_BinarySearchLeaf()` 의 리턴값이 `TRUE` 이면(index entry의 key값이 중복되면) `eDUPLICATEDKEY_BTM` 에러 리턴
    3. index entry를 삽입하는 데 필요한 free area 계산
        - `btm_LeafEntry` 의 크기 + slot의 크기
            - `btm_LeafEntry` 의 크기 = `BTM_LEAFENTRY_FIXED` + aligned key area length + sizeof(ObjectID)
    4. `BL_FREE` 값이 3에서 계산한 값보다 크면, 삽입 가능
        - `BL_CFREE` 값이 3에서 계산한 값보다 작으면 `edubtm_CompactLeafPage()`
        - contiguous free area에 index entry 복사
        - (1에서 정한 slot # + 1)에 해당하는 slot을 삽입한 index entry에 할당
        - page header 업데이트
    5. `BL_FIXED` 값이 3에서 계산한 값보다 작으면, page overflow이므로 `edubtm_SplitLeaf()`
    6. split돼서 새로 생긴 페이지를 가리키는 internal index entry를 반환
    
- edubtm_InsertInternal
    1. index entry를 삽입하는 데 필요한 free area 계산
        - `btm_InternalEntry` 의 크기 = `sizeof(spid)` + aligned length of (klen + data area)
        - 삽입하는 데 필요한 free area = `btm_InternalEntry` 의 크기 + `sizeof(slot)`
    2. `BI_FREE` 값이 1에서 계산한 값보다 작으면 (overflow)
        - `edubtm_SplitInternal()`
        - split된 페이지(새로 생긴 페이지)를 가리키는 internal index entry를 리턴
    3. `BI_FREE` 값이 1에서 계산한 값보다 크면
        - `BI_CFREE` 값이 1에서 계산한 값보다 작으면 `edubtm_CompactInternalPage()`
        - contiguous free area에 index entry 복사
        - slot array rearrange
            - slot #가  `high` 보다 큰 slot들의 offset 값을 shift
            - slot #가 `high + 1` 인 slot에 삽입한 index entry의 offset을 할당
        - page header 업데이트

- edubtm_SplitLeaf()
    1. 새로운 페이지를 allocate한다. `btm_AllocPage()`
    2. leaf page로 init한다. `edubtm_InitLeaf()`
    3. 새로 allocate한 페이지를 읽는다. `BfM_GetNewTrain()`
    4. overflow된 페이지의 index entry들을 두 페이지로 나눈다.
        1. fpage의 slot # 0에 해당하는 index entry부터 tpage에 차례대로 복사해 나간다.
            1. 만약 `sum` 이 `BL_HALF` 보다 크거나 같아지면, b로 이동한다.
            2. 만약 tpage의 slot #가 (high + 1)이라면, `sum` 만 증가시키고 index entry는 바로 삽입하지 않는다.
                
                `btm_LeafEntry`의 크기 = `BTM_LEAFENTRY_FIXED` + aligned key area size + sizeof(ObjectID)
                
            3. 만약 tpage의 slot #가 (high + 1)보다 크면, fpage에서 slot #가 1 작은 slot의 index entry를 복사한다.
            4. 만약 tpage의 slot #가 (high + 1)보다 작으면, fpage에서 slot #에 해당하는 index entry를 복사한다.
        2. 남은 index entry들을 allocate된 페이지에 채운다.
            1. fpage의 slot #를 하나씩 증가시키면서 해당하는 index entry를 npage에 복사한다.
            2. fpage의 slot #가 `high + 1` 이라면, index entry를 삽입한다.
        3. fpage를 compact하고(만약 필요하다면), cfree area에 index entry를 새로 삽입한다.
        4. `tpage` 를 `fpage` 에 복사한다.
        5. 각 페이지들의 header를 업데이트한다.
    5. allocate된 페이지를 leaf 페이지들의 doubly linked list에 추가한다.
    6. allocate된 페이지를 가리키는 internal item을 생성하고, 이를 리턴한다.
        1. internal item의 key value는 allocate된 페이지의 slot # 0이 가리키는 index entry의 key value이다.
        2. internal item의 spid는 allocate된 페이지의 페이지 #이다.
    7. overflow된 페이지의 dirty bit를 toggle한다. `BfM_SetDirty()`
    8. allocate된 페이지를 free한다. `BfM_FreeTrain()`

- edubtm_SplitInternal()
    1. 새로운 페이지를 할당 `btm_AllocPage()`
    2. 할당된 페이지를 internal 페이지로 initialize `edubtm_InitInternal()`
    3. 새로 allocate한 페이지를 읽는다. `BfM_GetNewTrain()`
    4. overflow된 페이지의 index entry들을 두 페이지로 나눈다.
        1. fpage의 slot # 0에 해당하는 index entry부터 tpage에 차례대로 복사해 나간다.
            1. 만약 `sum` 이 `BI_HALF` 보다 크거나 같아지면, b로 이동한다.
            2. 만약 tpage의 slot #가 (high + 1)이라면, `sum` 만 증가시키고 index entry는 바로 삽입하지 않는다.
                
                `btm_InternalEntry`의 크기 = sizeof(spid) + aligned length of (sizeof(Two) + klen)
                
            3. 만약 tpage의 slot #가 (high + 1)보다 크면, fpage에서 slot #가 1 작은 slot의 index entry를 복사한다.
            4. 만약 tpage의 slot #가 (high + 1)보다 작으면, fpage에서 slot #에 해당하는 index entry를 복사한다.
        2. tpage에 복사되지 않은 index entry들 중 키 값이 가장 작은 entry가 가리키는 spid를 새로 allocate된 페이지의 p0 값으로 설정
        3. tpage에 복사되지 않은 index entry들 중 키 값이 가장 작은 entry를 ritem으로 리턴, ritem의 spid를 새로 allocate된 페이지 #로 설정
        4. 남은 index entry들을 npage에 복사
        5. tpage에 item entry를 복사 (필요하다면)
            1. tpage를 compact (필요하다면) `edubtm_CompactInternalPage()`
            2. tpage hdr의 free를 slot # (high + 1)의 offset으로 설정
            3. tpage의 data 영역에 item entry 복사
        6. 각 페이지 header 업데이트
        7. allocate한 페이지를 free `BfM_FreeTrain()`
        
- edubtm_CompactLeafPage()
    1. slot #가 0에서부터 차례대로 data area에 복사한다. entry 간에 모든 free area는 제거한다. 그리고 entry의 offset을 업데이트한다.
    2. slotNo가 NIL이 아니면 마지막에 slotNo가 NIL인 entry를 복사한다.
    3. 페이지 header 업데이트
- edubtm_CompactInternalPage()
    
    
- edubtm_root_insert()
    1. 새로운 페이지를 allocate한다. `btm_AllocPage()`
    2. old root 페이지를 버퍼로 불러온다. `BfM_GetTrain()`
    3. allocate한 페이지를 버퍼로 불러온다. `BfM_GetNewTrain()`
    4. old root 페이지를 allocate된 페이지에 복사한다.
    5. old root 페이지를 new root page로 initialize한다. `edubtm_InitInternal()`
    6. split으로 인해 새로 생긴 페이지를 가리키는 internal item을 new root 페이지에 삽입한다.
    7. allocate된 페이지 #를 new root 페이지의 p0에 저장한다.
    8. new root 페이지의 children 페이지들이 leaf 페이지들이라면, leaf 페이지들 간에 doubly linked list를 생성해 준다.
    9. new root 페이지의 dirty bit을 toggle한다.
    10. 버퍼로 불러온 페이지를 free한다. `BfM_FreeTrain()`

- edubtm_KeyCompare()
    - key1과 key2에서 attribute value를 하나씩 뜯어내면서 값을 비교
        - type이 `SM_INT` 라면, key1과 key2에서 `SM_INT_SIZE` 만큼 읽어 값을 비교
        - type이 `SM_VARSTRING` 이라면, key1과 key2에서 length만큼 데이터를 읽어 값을 비교

- EduBtM_Fetch()
    - `startCompOp` == `SM_BOF` → `edubtm_FirstObject()`
    - `startCompOp` == `SM_EOF` → `edubtm_LastObject()`
    - `edubtm_Fetch()`

- edubtm_FirstObject()
    1. root 페이지를 읽는다. `BfM_GetTrain()`
    2. root 페이지의 p0에 해당하는 page #를 가진 페이지를 읽는다. `BfM_GetTrain()`
    3. root 페이지를 free한다. `BfM_FreeTrain()`
    4. Leaf page에 도달할 때까지 1, 2, 3의 과정을 반복한다.
    5. Leaf page의 slot # 0인 leaf index entry를 읽는다.

- edubtm_LastObject()
    1. root 페이지를 읽는다. `BfM_GetTrain()`
    2. root 페이지의 마지막 index entry(slot # = nSlots - 1)를 읽고, 그 index entry의 spid에 해당하는 page #를 가진 페이지를 읽는다. `BfM_GetTrain()`
    3. root 페이지를 free한다. `BfM_FreeTrain()`
    4. Leaf page에 도달할 때까지 1, 2, 3의 과정을 반복한다.
    5. Leaf page의 마지막 index entry를 읽는다.

- EduBtM_DeleteObject()
    - `edubtm_Delete()`
    - root에서 underflow가 발생하면, `btm_root_delete()`
    - root가 split되면, `edubtm_root_insert()`

- edubtm_Fetch()
    1. root 페이지를 읽는다. `BfM_GetTrain()`
    2. root가 LEAF page이면,
        1. `startKval` 과 start operation을 만족하는 키와 가장 가까운 키를 가진 index entry 찾는다.
            1. `startKval` 과 같거나 작은 키들 중 가장 큰 키를 찾는다. `edubtm_BinarySearchLeaf()`
            2. 만약 start operation이 `EQ` 이고, 같은 키가 존재하면 ok
            3. 만약 start operation이 `EQ` 이고, 같은 키가 존재하지 않으면 EOS
            4. 만약 start operation이 `LE` 이고, 같은 키가 존재하면 ok
            5. 만약 start operation이 `LE` 이고, 같은 키가 존재하지 않으면 ok
            6. 만약 start operation이 `LT` 이고, 같은 키가 존재하면 -1
            7. 만약 start operation이 `LT` 이고, 같은 키가 존재하지 않으면 ok
            8. 만약 start operation이 `GE` 이고, 같은 키가 존재하면 ok
            9. 만약 start operation이 `GE` 이고, 같은 키가 존재하지 않으면 +1
            10. 만약 start operation이 `GT` 이고, 같은 키가 존재하면 +1
            11. 만약 start operation이 `GT` 이고, 같은 키가 존재하지 않으면 +1
        2. 해당 index entry를 가리키는 cursor를 리턴한다.
    3. root가 INTERNAL page이면,
    
- EduBtM_FetchNext()
    
    
- edubtm_Delete()
    - root 페이지가 INTERNAL 페이지면,
        - 
    - root 페이지가 LEAF 페이지면,
        - `edubtm_DeleteLeaf()`
        - 만약 underflow가 발생하면, `BL_FREE` > (`PAGESIZE - BL_FIXED` / 2) → f = TRUE

- edubtm_FreePages()