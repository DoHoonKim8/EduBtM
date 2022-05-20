/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module: edubtm_Split.c
 *
 * Description : 
 *  This file has three functions about 'split'.
 *  'edubtm_SplitInternal(...) and edubtm_SplitLeaf(...) insert the given item
 *  after spliting, and return 'ritem' which should be inserted into the
 *  parent page.
 *
 * Exports:
 *  Four edubtm_SplitInternal(ObjectID*, BtreeInternal*, Two, InternalItem*, InternalItem*)
 *  Four edubtm_SplitLeaf(ObjectID*, PageID*, BtreeLeaf*, Two, LeafItem*, InternalItem*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "EduBtM_Internal.h"



/*@================================
 * edubtm_SplitInternal()
 *================================*/
/*
 * Function: Four edubtm_SplitInternal(ObjectID*, BtreeInternal*,Two, InternalItem*, InternalItem*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  At first, the function edubtm_SplitInternal(...) allocates a new internal page
 *  and initialize it.  Secondly, all items in the given page and the given
 *  'item' are divided by halves and stored to the two pages.  By spliting,
 *  the new internal item should be inserted into their parent and the item will
 *  be returned by 'ritem'.
 *
 *  A temporary page is used because it is difficult to use the given page
 *  directly and the temporary page will be copied to the given page later.
 *
 * Returns:
 *  error code
 *    some errors caused by function calls
 *
 * Note:
 *  The caller should call BfM_SetDirty() for 'fpage'.
 */
Four edubtm_SplitInternal(
    ObjectID                    *catObjForFile,         /* IN catalog object of B+ tree file */
    BtreeInternal               *fpage,                 /* INOUT the page which will be splitted */
    Two                         high,                   /* IN slot No. for the given 'item' */
    InternalItem                *item,                  /* IN the item which will be inserted */
    InternalItem                *ritem)                 /* OUT the item which will be returned by spliting */
{
    Four                        e;                      /* error number */
    Two                         i;                      /* slot No. in the given page, fpage */
    Two                         j;                      /* slot No. in the splitted pages */
    Two                         k;                      /* slot No. in the new page */
    Two                         maxLoop;                /* # of max loops; # of slots in fpage + 1 */
    Four                        sum;                    /* the size of a filled area */
    Boolean                     flag=FALSE;             /* TRUE if 'item' become a member of fpage */
    PageID                      newPid;                 /* for a New Allocated Page */
    BtreeInternal               *tpage;
    BtreeInternal               *npage;                 /* a page pointer for the new allocated page */
    Two                         fEntryOffset;           /* starting offset of an entry in fpage */
    Two                         nEntryOffset;           /* starting offset of an entry in npage */
    Two                         entryLen;               /* length of an entry */
    btm_InternalEntry           *fEntry;                /* internal entry in the given page, fpage */
    btm_InternalEntry           *nEntry;                /* internal entry in the new page, npage*/
    Boolean                     isTmp;

    isTmp = FALSE;
    e = btm_AllocPage(catObjForFile, &fpage->hdr.pid, &newPid);
    if (e < 0) ERR( e );

    e = edubtm_InitInternal(&newPid, FALSE, isTmp);
    if (e < 0) ERR( e );

    e = BfM_GetNewTrain(&newPid, (char**)&npage, PAGE_BUF);
    if (e < 0) ERR( e );

    memcpy(&tpage, fpage, PAGESIZE); 

    maxLoop = fpage->hdr.nSlots + 1;
    sum = 0;
    i = 0; // slot No. in `fpage`
    j = 0; // slot No. in `tpage`

    for ( ; j < maxLoop; j++) {
        if (sum > BI_HALF) {
            break;
        }
        if (j == high + 1) {
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen);
            isTmp = TRUE;
        } else {
            fEntry = (btm_InternalEntry*)&(fpage->data[fpage->slot[-i]]);
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + fEntry->klen);

            tpage->slot[-j] = fpage->slot[-i];
            memcpy(&tpage->data[tpage->slot[-j]], fEntry, entryLen);
            i++;
        }
        sum += entryLen + sizeof(Two);
    }
    
    tpage->hdr.nSlots = j;
    if (tpage->hdr.type & ROOT) {
        tpage->hdr.type ^= ROOT;
    }

    if (i == high + 1) {
        ritem->spid = newPid.pageNo;
        ritem->klen = item->klen;
        memcpy(ritem->kval, item->kval, item->klen);
    } else {
        fEntry = (btm_InternalEntry*)&(fpage->data[fpage->slot[-i]]);
        ritem->spid = newPid.pageNo;
        ritem->klen = fEntry->klen;
        memcpy(ritem->kval, fEntry->kval, fEntry->klen);
        i++;
    }

    fEntry = (btm_InternalEntry*)&(fpage->data[fpage->slot[-i]]); 
    npage->hdr.p0 = fEntry->spid; 

    k = 0;
    nEntryOffset = 0;
    for ( ; j + k < maxLoop - 1; k++) {
        if (i == high + 1) {
            npage->slot[-k] = nEntryOffset;
            nEntry = (btm_InternalEntry*)&npage->data[npage->slot[-k]];
            nEntry->klen = item->klen;
            nEntry->spid = item->spid;
            memcpy(nEntry->kval, item->kval, item->klen);

            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + nEntry->klen);
        } else {
            fEntry = (btm_InternalEntry*)&(fpage->data[fpage->slot[-i]]);
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + fEntry->klen);

            npage->slot[-k] = nEntryOffset;
            memcpy(&npage->data[npage->slot[-k]], fEntry, entryLen);
            i++;
        }
        nEntryOffset += entryLen;
    }

    if (isTmp == TRUE) {
        if (sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen) > BI_CFREE(tpage)) {
            edubtm_CompactInternalPage(tpage, NIL);
        }
        fEntry->klen = item->klen;
        fEntry->spid = item->spid;
        memcpy(fEntry->kval, item->kval, item->klen);

        entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen); 

        tpage->slot[-(high + 1)] = tpage->hdr.free;
        memcpy(&tpage->data[tpage->hdr.free], fEntry, entryLen);

        tpage->hdr.free += entryLen;
        tpage->hdr.nSlots++;
    }

    memcpy(fpage, tpage, PAGESIZE);

    npage->hdr.free = nEntryOffset;
    npage->hdr.nSlots = k;

    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if (e < 0) ERR( e );

    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if (e < 0) ERR( e );
    
    return(eNOERROR);
    
} /* edubtm_SplitInternal() */



/*@================================
 * edubtm_SplitLeaf()
 *================================*/
/*
 * Function: Four edubtm_SplitLeaf(ObjectID*, PageID*, BtreeLeaf*, Two, LeafItem*, InternalItem*)
 *
 * Description: 
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  The function edubtm_SplitLeaf(...) is similar to edubtm_SplitInternal(...) except
 *  that the entry of a leaf differs from the entry of an internal and the first
 *  key value of a new page is used to make an internal item of their parent.
 *  Internal pages do not maintain the linked list, but leaves do it, so links
 *  are properly updated.
 *
 * Returns:
 *  Error code
 *  eDUPLICATEDOBJECTID_BTM
 *    some errors caused by function calls
 *
 * Note:
 *  The caller should call BfM_SetDirty() for 'fpage'.
 */
Four edubtm_SplitLeaf(
    ObjectID                    *catObjForFile, /* IN catalog object of B+ tree file */
    PageID                      *root,          /* IN PageID for the given page, 'fpage' */
    BtreeLeaf                   *fpage,         /* INOUT the page which will be splitted */
    Two                         high,           /* IN slotNo for the given 'item' */
    LeafItem                    *item,          /* IN the item which will be inserted */
    InternalItem                *ritem)         /* OUT the item which will be returned by spliting */
{
    Four                        e;              /* error number */
    Two                         i;              /* slot No. in the given page, fpage */
    Two                         j;              /* slot No. in the splitted pages */
    Two                         k;              /* slot No. in the new page */
    Two                         maxLoop;        /* # of max loops; # of slots in fpage + 1 */
    Four                        sum;            /* the size of a filled area */
    PageID                      newPid;         /* for a New Allocated Page */
    PageID                      nextPid;        /* for maintaining doubly linked list */
    BtreeLeaf                   tpage;          /* a temporary page for the given page */
    BtreeLeaf                   *npage;         /* a page pointer for the new page */
    BtreeLeaf                   *mpage;         /* for doubly linked list */
    btm_LeafEntry               *itemEntry;     /* entry for the given 'item' */
    btm_LeafEntry               *fEntry;        /* an entry in the given page, 'fpage' */
    btm_LeafEntry               *nEntry;        /* an entry in the new page, 'npage' */
    ObjectID                    *iOidArray;     /* ObjectID array of 'itemEntry' */
    ObjectID                    *fOidArray;     /* ObjectID array of 'fEntry' */
    Two                         fEntryOffset;   /* starting offset of 'fEntry' */
    Two                         nEntryOffset;   /* starting offset of 'nEntry' */
    Two                         oidArrayNo;     /* element No in an ObjectID array */
    Two                         alignedKlen;    /* aligned length of the key length */
    Two                         itemEntryLen;   /* length of entry for item */
    Two                         entryLen;       /* entry length */
    Boolean                     flag;
    Boolean                     isTmp;
 
    isTmp = FALSE;
    e = btm_AllocPage(catObjForFile, root, &newPid);
    if (e < 0) ERR( e );

    e = edubtm_InitLeaf(&newPid, FALSE, isTmp);
    if (e < 0) ERR( e );

    e = BfM_GetNewTrain(&newPid, (char**)&npage, PAGE_BUF);
    if (e < 0) ERR( e );

    memcpy(&tpage, fpage, PAGESIZE);

    maxLoop = fpage->hdr.nSlots + 1;
    sum = 0;
    i = 0; // slot No. in `fpage`
    j = 0; // slot No. in `tpage`

    for ( ; j < maxLoop; j++) {
        if (sum >= BL_HALF) break;
        if (j == high + 1) {
            alignedKlen = ALIGNED_LENGTH(item->klen);
            entryLen = BTM_LEAFENTRY_FIXED + alignedKlen + sizeof(ObjectID);
            
            isTmp = TRUE; // insert index entry in `tpage`
            itemEntryLen = entryLen;
        } else {
            fEntry = (btm_LeafEntry*)&(fpage->data[fpage->slot[-i]]);
            alignedKlen = ALIGNED_LENGTH(fEntry->klen);
            entryLen = BTM_LEAFENTRY_FIXED + alignedKlen + sizeof(ObjectID);
            tpage.slot[-j] = fpage->slot[-i];
            memcpy(&tpage.data[tpage.slot[-j]], fEntry, entryLen);
            i++;
        }
        sum += entryLen + sizeof(Two);
    }

    tpage.hdr.nSlots = j;

    if (tpage.hdr.type & ROOT) {
        tpage.hdr.type ^= ROOT;
    }

    k = 0; // slot No. in `npage`
    nEntryOffset = 0;
    for ( ; j + k < maxLoop; k++) {
        if (i == high + 1) {
            npage->slot[-k] = nEntryOffset;
            itemEntry = (btm_LeafEntry*)&npage->data[nEntryOffset];
            itemEntry->klen = item->klen;
            itemEntry->nObjects = 1;
            memcpy(itemEntry->kval, item->kval, item->klen);

            alignedKlen = ALIGNED_LENGTH(itemEntry->klen);
            entryLen = BTM_LEAFENTRY_FIXED + alignedKlen + sizeof(ObjectID);
            *(ObjectID*)&itemEntry->kval[alignedKlen] = item->oid;
        } else {
            fEntry = (btm_LeafEntry*)&(fpage->data[fpage->slot[-i]]);
            alignedKlen = ALIGNED_LENGTH(fEntry->klen);
            entryLen = BTM_LEAFENTRY_FIXED + alignedKlen + sizeof(ObjectID);
            memcpy(&npage->data[nEntryOffset], fEntry, entryLen);
            npage->slot[-k] = nEntryOffset;
            i++;
        }
        nEntryOffset += entryLen;
    }

    if (isTmp == TRUE) {
        if (BL_CFREE(&tpage) < itemEntryLen) {
            edubtm_CompactLeafPage(&tpage, NIL);
        }
        tpage.slot[-(high + 1)] = tpage.hdr.free;
        itemEntry = &tpage.data[tpage.slot[-(high + 1)]];
        itemEntry->nObjects = 1;
        itemEntry->klen = item->klen;
        memcpy(itemEntry->kval, item->kval, item->klen);
        *(ObjectID*)&itemEntry->kval[ALIGNED_LENGTH(item->klen)] = item->oid;

        tpage.hdr.free += itemEntryLen;
        tpage.hdr.nSlots++;
    }

    memcpy(fpage, &tpage, PAGESIZE);
    
    npage->hdr.prevPage = root->pageNo;
    npage->hdr.nextPage = fpage->hdr.nextPage;

    fpage->hdr.prevPage;
    fpage->hdr.nextPage = newPid.pageNo;

    if(npage->hdr.nextPage != NIL){
        nextPid.pageNo = npage->hdr.nextPage;
        nextPid.volNo = npage->hdr.pid.volNo;

        e = BfM_GetTrain(&nextPid, &mpage, PAGE_BUF);
        if(e<0) ERR(e);
    
        mpage->hdr.prevPage = newPid.pageNo;

        e = BfM_SetDirty(&nextPid, PAGE_BUF);
        if(e<0) ERR(e);

        e = BfM_FreeTrain(&nextPid, PAGE_BUF);
        if(e<0) ERR(e);

    }

    nEntry = &npage->data[npage->slot[0]]; 
    ritem->spid = newPid.pageNo;
    ritem->klen = nEntry->klen;
    memcpy(ritem->kval, nEntry->kval, nEntry->klen);

    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if(e<0) ERR(e);

    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if(e<0) ERR(e);

    return(eNOERROR);
    
} /* edubtm_SplitLeaf() */
