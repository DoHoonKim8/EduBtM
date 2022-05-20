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
    ObjectID *catObjForFile, /* IN catalog object of B+ tree file */
    BtreeInternal *fpage,    /* INOUT the page which will be splitted */
    Two high,                /* IN slot No. for the given 'item' */
    InternalItem *item,      /* IN the item which will be inserted */
    InternalItem *ritem)     /* OUT the item which will be returned by spliting */
{
    Four e;                   /* error number */
    Two i;                    /* slot No. in the splitted pages */
    Two j;                    /* slot No. in the given page, fpage */
    Two k;                    /* slot No. in the new page */
    Two maxLoop;              /* # of max loops; # of slots in fpage + 1 */
    Four sum;                 /* the size of a filled area */
    Boolean f = FALSE;
    PageID newPid;            /* for a New Allocated Page */
    BtreeInternal *npage;     /* a page pointer for the new allocated page */
    Two fEntryOffset;         /* starting offset of an entry in fpage */
    Two nEntryOffset;         /* starting offset of an entry in npage */
    Two entryLen;             /* length of an entry */
    Two itemEntryLen;
    btm_InternalEntry *fEntry; /* internal entry in the given page, fpage */
    btm_InternalEntry *nEntry; /* internal entry in the new page, npage*/
    btm_InternalEntry *itemEntry;
    Boolean isTmp;

    e = btm_AllocPage(catObjForFile, &fpage->hdr.pid, &newPid);
    if (e < 0)
        ERR(e);

    e = edubtm_InitInternal(&newPid, FALSE, FALSE);
    if (e < 0)
        ERR(e);

    e = BfM_GetNewTrain(&newPid, &npage, PAGE_BUF);
    if (e < 0)
        ERR(e);

    itemEntryLen = 4 + ALIGNED_LENGTH(2 + item->klen);

    maxLoop = fpage->hdr.nSlots + 1;
    sum = 0;
    i = 0;
    j = 0;
    f = FALSE;
    for (i = 0; i < maxLoop && sum < BI_HALF; i++)
    {

        if (i == high + 1)
        {                
            f = TRUE;
            entryLen = itemEntryLen;
        }
        else
        {
            fEntryOffset = fpage->slot[-j];
            fEntry = &fpage->data[fEntryOffset];
            entryLen = (4 + ALIGNED_LENGTH(2 + fEntry->klen));
            j++;
        }

        sum += entryLen + sizeof(Two);
    }
    fpage->hdr.nSlots = j;
    if (fpage->hdr.type & ROOT)
    {
        fpage->hdr.type ^= ROOT;
    }

    k = -1;
    nEntryOffset = 0;
    for (; i < maxLoop; i++)
    {

        if (k != -1)
        {
            nEntryOffset = npage->hdr.free;
            npage->slot[-k] = nEntryOffset;
            nEntry = &npage->data[nEntryOffset];
        }

        if (i == high + 1)
        {

            if (k != -1)
            {
                itemEntry = nEntry;
                memcpy(itemEntry, item, itemEntryLen);
            }
            else
            {
                memcpy(ritem, item, itemEntryLen);
            }

            entryLen = itemEntryLen;
        }
        else
        {
            fEntryOffset = fpage->slot[-j];
            fEntry = &fpage->data[fEntryOffset];
            entryLen = (4 + ALIGNED_LENGTH(2 + fEntry->klen));

            if (k != -1)
            {
                memcpy(nEntry, fEntry, entryLen);
            }
            else
            {
                memcpy(ritem, fEntry, entryLen);
            }

            if (fEntryOffset + entryLen == fpage->hdr.free)
                fpage->hdr.free -= entryLen;
            else
                fpage->hdr.unused += entryLen;

            j++;
        }

        if (k == -1)
        {
            npage->hdr.p0 = ritem->spid;
            ritem->spid = newPid.pageNo;
        }
        else
        {
            npage->hdr.free += entryLen;
        }

        k++;
    }
    npage->hdr.nSlots = k;

    if (f == TRUE)
    {
        if (itemEntryLen > BL_CFREE(fpage))
        {
            edubtm_CompactInternalPage(fpage, NIL);
        }
        for (i = fpage->hdr.nSlots - 1; i > high; i--)
        {
            fpage->slot[-(i + 1)] = fpage->slot[-i];
        }
        fpage->slot[-(high + 1)] = fpage->hdr.free;

        fEntryOffset = fpage->hdr.free;
        fEntry = &fpage->data[fEntryOffset];
        itemEntry = fEntry;

        memcpy(itemEntry, item, itemEntryLen);

        fpage->hdr.free += itemEntryLen;
        fpage->hdr.nSlots += 1;
    }

    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if (e < 0)
        ERR(e);

    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if (e < 0)
        ERR(e);

    return (eNOERROR);

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
    ObjectID *catObjForFile, /* IN catalog object of B+ tree file */
    PageID *root,            /* IN PageID for the given page, 'fpage' */
    BtreeLeaf *fpage,        /* INOUT the page which will be splitted */
    Two high,                /* IN slotNo for the given 'item' */
    LeafItem *item,          /* IN the item which will be inserted */
    InternalItem *ritem)     /* OUT the item which will be returned by spliting */
{
    Four e;                   /* error number */
    Two i;                    /* slot No. in the splitted pages */
    Two j;                    /* slot No. in the given page, fpage */
    Two k;                    /* slot No. in the new page */
    Two maxLoop;              /* # of max loops; # of slots in fpage + 1 */
    Four sum;                 /* the size of a filled area */
    PageID newPid;            /* for a New Allocated Page */
    PageID nextPid;           /* for maintaining doubly linked list */
    BtreeLeaf tpage;          /* a temporary page for the given page */
    BtreeLeaf *npage;         /* a page pointer for the new page */
    BtreeLeaf *mpage;         /* for doubly linked list */
    btm_LeafEntry *itemEntry; /* entry for the given 'item' */
    btm_LeafEntry *fEntry;    /* an entry in the given page, 'fpage' */
    btm_LeafEntry *nEntry;    /* an entry in the new page, 'npage' */
    ObjectID *iOidArray;      /* ObjectID array of 'itemEntry' */
    ObjectID *fOidArray;      /* ObjectID array of 'fEntry' */
    Two fEntryOffset;         /* starting offset of 'fEntry' */
    Two nEntryOffset;         /* starting offset of 'nEntry' */
    Two oidArrayNo;           /* element No in an ObjectID array */
    Two alignedKlen;          /* aligned length of the key length */
    Two itemEntryLen;         /* length of entry for item */
    Two entryLen;             /* entry length */
    Boolean flag;
    Boolean isTmp;
    Boolean f;

    e = btm_AllocPage(catObjForFile, &fpage->hdr.pid, &newPid);
    if (e < 0)
        ERR(e);

    e = edubtm_InitLeaf(&newPid, FALSE, FALSE);
    if (e < 0)
        ERR(e);

    e = BfM_GetTrain(&newPid, &npage, PAGE_BUF);
    if (e < 0)
        ERR(e);

    alignedKlen = ALIGNED_LENGTH(item->klen);
    itemEntryLen = (2 + 2 + alignedKlen + sizeof(ObjectID));

    maxLoop = fpage->hdr.nSlots + 1;
    sum = 0;
    i = 0;
    j = 0;
    f = FALSE;

    for (i = 0; i < maxLoop && sum < BL_HALF; i++)
    {

        if (i == high + 1)
        {
            f = TRUE;
            entryLen = itemEntryLen;
        }
        else
        {
            fEntryOffset = fpage->slot[-j];
            fEntry = &fpage->data[fEntryOffset];

            entryLen = (2 + 2 + ALIGNED_LENGTH(fEntry->klen) + sizeof(ObjectID));

            j++;
        }

        sum += entryLen + sizeof(Two);
    }
    fpage->hdr.nSlots = j;
    if (fpage->hdr.type & ROOT)
    {
        fpage->hdr.type ^= ROOT;
    }
    k = 0;
    nEntryOffset = 0;

    for (; i < maxLoop; i++)
    {
        nEntryOffset = npage->hdr.free;
        npage->slot[-k] = nEntryOffset;
        nEntry = &npage->data[nEntryOffset];

        if (i == high + 1)
        {

            itemEntry = nEntry;

            itemEntry->nObjects = item->nObjects;
            itemEntry->klen = item->klen;
            memcpy(itemEntry->kval, item->kval, item->klen);
            iOidArray = &itemEntry->kval[alignedKlen];
            *iOidArray = item->oid;

            entryLen = itemEntryLen;
        }
        else
        {
            fEntryOffset = fpage->slot[-j];
            fEntry = &fpage->data[fEntryOffset];
            entryLen = (2 + 2 + ALIGNED_LENGTH(fEntry->klen) + sizeof(ObjectID));

            memcpy(nEntry, fEntry, entryLen);

            if (fEntryOffset + entryLen == fpage->hdr.free)
            {
                fpage->hdr.free -= entryLen;
            }
            else
            {
                fpage->hdr.unused += entryLen;
            }

            j++;
        }

        npage->hdr.free += entryLen;

        k++;
    }
    npage->hdr.nSlots = k;

    if (f == TRUE)
    {
        if (itemEntryLen > BL_CFREE(fpage))
        {
            edubtm_CompactLeafPage(fpage, NIL);
        }
        for (i = fpage->hdr.nSlots - 1; i > high; i--)
        {
            fpage->slot[-(i + 1)] = fpage->slot[-i];
        }
        fpage->slot[-(high + 1)] = fpage->hdr.free;

        fEntryOffset = fpage->hdr.free;
        fEntry = &fpage->data[fEntryOffset];
        itemEntry = fEntry;

        itemEntry->nObjects = item->nObjects;
        itemEntry->klen = item->klen;
        memcpy(itemEntry->kval, item->kval, item->klen);
        iOidArray = &itemEntry->kval[alignedKlen];
        *iOidArray = item->oid;

        fpage->hdr.free += itemEntryLen;
        fpage->hdr.nSlots += 1;
    }

    npage->hdr.prevPage = root->pageNo;
    npage->hdr.nextPage = fpage->hdr.nextPage;
    fpage->hdr.prevPage;
    fpage->hdr.nextPage = newPid.pageNo;

    if (npage->hdr.nextPage != NIL)
    {
        nextPid.pageNo = npage->hdr.nextPage;
        nextPid.volNo = npage->hdr.pid.volNo;

        e = BfM_GetTrain(&nextPid, &mpage, PAGE_BUF);
        if (e < 0)
            ERR(e);

        mpage->hdr.prevPage = newPid.pageNo;

        e = BfM_SetDirty(&nextPid, PAGE_BUF);
        if (e < 0)
            ERR(e);

        e = BfM_FreeTrain(&nextPid, PAGE_BUF);
        if (e < 0)
            ERR(e);
    }

    nEntry = &npage->data[npage->slot[0]];
    ritem->spid = newPid.pageNo;
    ritem->klen = nEntry->klen;
    memcpy(ritem->kval, nEntry->kval, nEntry->klen);

    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if (e < 0)
        ERR(e);

    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if (e < 0)
        ERR(e);

    return (eNOERROR);

} /* edubtm_SplitLeaf() */
