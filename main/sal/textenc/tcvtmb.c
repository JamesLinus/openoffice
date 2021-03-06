/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#include "tenchelp.h"
#include "unichars.h"
#include "rtl/textcvt.h"

/* ======================================================================= */

/* DBCS to Unicode conversion routine use a lead table for the first byte, */
/* where we determine the trail table or for single byte chars the unicode */
/* value. We have for all lead byte a separate table, because we can */
/* then share many tables for different charset encodings. */

/* ======================================================================= */

sal_Size ImplDBCSToUnicode( const ImplTextConverterData* pData, void* pContext,
                            const sal_Char* pSrcBuf, sal_Size nSrcBytes,
                            sal_Unicode* pDestBuf, sal_Size nDestChars,
                            sal_uInt32 nFlags, sal_uInt32* pInfo,
                            sal_Size* pSrcCvtBytes )
{
    sal_uChar                   cLead;
    sal_uChar                   cTrail;
    sal_Unicode                 cConv;
    const ImplDBCSToUniLeadTab* pLeadEntry;
    const ImplDBCSConvertData*  pConvertData = (const ImplDBCSConvertData*)pData;
    const ImplDBCSToUniLeadTab* pLeadTab = pConvertData->mpToUniLeadTab;
    sal_Unicode*                pEndDestBuf;
    const sal_Char*             pEndSrcBuf;

    (void) pContext; /* unused */

    *pInfo = 0;
    pEndDestBuf = pDestBuf+nDestChars;
    pEndSrcBuf  = pSrcBuf+nSrcBytes;
    while ( pSrcBuf < pEndSrcBuf )
    {
        cLead = (sal_uChar)*pSrcBuf;

        /* get entry for the lead byte */
        pLeadEntry = pLeadTab+cLead;

        /* SingleByte char? */
        if (pLeadEntry->mpToUniTrailTab == NULL
            || cLead < pConvertData->mnLeadStart
            || cLead > pConvertData->mnLeadEnd)
        {
            cConv = pLeadEntry->mnUniChar;
            if ( !cConv && (cLead != 0) )
            {
                *pInfo |= RTL_TEXTTOUNICODE_INFO_UNDEFINED;
                if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_MASK) == RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_ERROR )
                {
                    *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR;
                    break;
                }
                else if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_MASK) == RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_IGNORE )
                {
                    pSrcBuf++;
                    continue;
                }
                else
                    cConv = ImplGetUndefinedUnicodeChar(cLead, nFlags);
            }
        }
        else
        {
            /* Source buffer to small */
            if ( pSrcBuf +1 == pEndSrcBuf )
            {
                *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR | RTL_TEXTTOUNICODE_INFO_SRCBUFFERTOSMALL;
                break;
            }

            pSrcBuf++;
            cTrail = (sal_uChar)*pSrcBuf;
            if ( (cTrail >= pLeadEntry->mnTrailStart) && (cTrail <= pLeadEntry->mnTrailEnd) )
                cConv = pLeadEntry->mpToUniTrailTab[cTrail-pLeadEntry->mnTrailStart];
            else
                cConv = 0;

            if ( !cConv )
            {
                /* EUDC Ranges */
                sal_uInt16              i;
                const ImplDBCSEUDCData* pEUDCTab = pConvertData->mpEUDCTab;
                for ( i = 0; i < pConvertData->mnEUDCCount; i++ )
                {
                    if ( (cLead >= pEUDCTab->mnLeadStart) &&
                         (cLead <= pEUDCTab->mnLeadEnd) )
                    {
                        sal_uInt16 nTrailCount = 0;
                        if ( (cTrail >= pEUDCTab->mnTrail1Start) &&
                             (cTrail <= pEUDCTab->mnTrail1End) )
                        {
                            cConv = pEUDCTab->mnUniStart+
                                    ((cLead-pEUDCTab->mnLeadStart)*pEUDCTab->mnTrailRangeCount)+
                                    (cTrail-pEUDCTab->mnTrail1Start);
                            break;
                        }
                        else
                        {
                            nTrailCount = pEUDCTab->mnTrail1End-pEUDCTab->mnTrail1Start+1;
                            if ( (pEUDCTab->mnTrailCount >= 2) &&
                                 (cTrail >= pEUDCTab->mnTrail2Start) &&
                                 (cTrail <= pEUDCTab->mnTrail2End) )
                            {
                                cConv = pEUDCTab->mnUniStart+
                                        ((cLead-pEUDCTab->mnLeadStart)*pEUDCTab->mnTrailRangeCount)+
                                        nTrailCount+
                                        (cTrail-pEUDCTab->mnTrail2Start);
                                break;
                            }
                            else
                            {
                                nTrailCount = pEUDCTab->mnTrail2End-pEUDCTab->mnTrail2Start+1;
                                if ( (pEUDCTab->mnTrailCount >= 3) &&
                                     (cTrail >= pEUDCTab->mnTrail3Start) &&
                                     (cTrail <= pEUDCTab->mnTrail3End) )
                                {
                                    cConv = pEUDCTab->mnUniStart+
                                            ((cLead-pEUDCTab->mnLeadStart)*pEUDCTab->mnTrailRangeCount)+
                                            nTrailCount+
                                            (cTrail-pEUDCTab->mnTrail3Start);
                                    break;
                                }
                            }
                        }
                    }

                    pEUDCTab++;
                }

                if ( !cConv )
                {
                    /* Wir vergleichen den kompletten Trailbereich den wir */
                    /* definieren, der normalerweise groesser sein kann als */
                    /* der definierte. Dies machen wir, damit Erweiterungen von */
                    /* uns nicht beruecksichtigten Encodings so weit wie */
                    /* moeglich auch richtig zu behandeln, das double byte */
                    /* characters auch als ein einzelner Character behandelt */
                    /* wird. */
                    if (cLead < pConvertData->mnLeadStart
                        || cLead > pConvertData->mnLeadEnd
                        || cTrail < pConvertData->mnTrailStart
                        || cTrail > pConvertData->mnTrailEnd)
                    {
                        *pInfo |= RTL_TEXTTOUNICODE_INFO_INVALID;
                        if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_INVALID_MASK) == RTL_TEXTTOUNICODE_FLAGS_INVALID_ERROR )
                        {
                            *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR;
                            break;
                        }
                        else if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_INVALID_MASK) == RTL_TEXTTOUNICODE_FLAGS_INVALID_IGNORE )
                        {
                            pSrcBuf++;
                            continue;
                        }
                        else
                            cConv = RTL_TEXTENC_UNICODE_REPLACEMENT_CHARACTER;
                    }
                    else
                    {
                        *pInfo |= RTL_TEXTTOUNICODE_INFO_MBUNDEFINED;
                        if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_MASK) == RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_ERROR )
                        {
                            *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR;
                            break;
                        }
                        else if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_MASK) == RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_IGNORE )
                        {
                            pSrcBuf++;
                            continue;
                        }
                        else
                            cConv = RTL_TEXTENC_UNICODE_REPLACEMENT_CHARACTER;
                    }
                }
            }
        }

        if ( pDestBuf == pEndDestBuf )
        {
            *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR | RTL_TEXTTOUNICODE_INFO_DESTBUFFERTOSMALL;
            break;
        }

        *pDestBuf = cConv;
        pDestBuf++;
        pSrcBuf++;
    }

    *pSrcCvtBytes = nSrcBytes - (pEndSrcBuf-pSrcBuf);
    return (nDestChars - (pEndDestBuf-pDestBuf));
}

/* ----------------------------------------------------------------------- */

sal_Size ImplUnicodeToDBCS( const ImplTextConverterData* pData, void* pContext,
                            const sal_Unicode* pSrcBuf, sal_Size nSrcChars,
                            sal_Char* pDestBuf, sal_Size nDestBytes,
                            sal_uInt32 nFlags, sal_uInt32* pInfo,
                            sal_Size* pSrcCvtChars )
{
    sal_uInt16                  cConv;
    sal_Unicode                 c;
    sal_uChar                   nHighChar;
    sal_uChar                   nLowChar;
    const ImplUniToDBCSHighTab* pHighEntry;
    const ImplDBCSConvertData*  pConvertData = (const ImplDBCSConvertData*)pData;
    const ImplUniToDBCSHighTab* pHighTab = pConvertData->mpToDBCSHighTab;
    sal_Char*                   pEndDestBuf;
    const sal_Unicode*          pEndSrcBuf;

    sal_Bool bCheckRange = (pConvertData->mnLeadStart != 0
                            || pConvertData->mnLeadEnd != 0xFF);
        /* this statement has the effect that this extra check is only done for
           EUC-KR, which uses the MS-949 tables, but does not support the full
           range of MS-949 */

    (void) pContext; /* unused */

    *pInfo = 0;
    pEndDestBuf = pDestBuf+nDestBytes;
    pEndSrcBuf  = pSrcBuf+nSrcChars;
    while ( pSrcBuf < pEndSrcBuf )
    {
        c = *pSrcBuf;
        nHighChar = (sal_uChar)((c >> 8) & 0xFF);
        nLowChar = (sal_uChar)(c & 0xFF);

        /* get entry for the high byte */
        pHighEntry = pHighTab+nHighChar;

        /* is low byte in the table range */
        if ( (nLowChar >= pHighEntry->mnLowStart) && (nLowChar <= pHighEntry->mnLowEnd) )
        {
            cConv = pHighEntry->mpToUniTrailTab[nLowChar-pHighEntry->mnLowStart];
            if (bCheckRange && cConv > 0x7F
                && ((cConv >> 8) < pConvertData->mnLeadStart
                    || (cConv >> 8) > pConvertData->mnLeadEnd
                    || (cConv & 0xFF) < pConvertData->mnTrailStart
                    || (cConv & 0xFF) > pConvertData->mnTrailEnd))
                cConv = 0;
        }
        else
            cConv = 0;

        if (cConv == 0 && c != 0)
        {
            /* Map to EUDC ranges: */
            ImplDBCSEUDCData const * pEUDCTab = pConvertData->mpEUDCTab;
            sal_uInt32 i;
            for (i = 0; i < pConvertData->mnEUDCCount; ++i)
            {
                if (c >= pEUDCTab->mnUniStart && c <= pEUDCTab->mnUniEnd)
                {
                    sal_uInt32 nIndex = c - pEUDCTab->mnUniStart;
                    sal_uInt32 nLeadOff
                        = nIndex / pEUDCTab->mnTrailRangeCount;
                    sal_uInt32 nTrailOff
                        = nIndex % pEUDCTab->mnTrailRangeCount;
                    sal_uInt32 nSize;
                    cConv = (sal_uInt16)
                                ((pEUDCTab->mnLeadStart + nLeadOff) << 8);
                    nSize
                        = pEUDCTab->mnTrail1End - pEUDCTab->mnTrail1Start + 1;
                    if (nTrailOff < nSize)
                    {
                        cConv |= pEUDCTab->mnTrail1Start + nTrailOff;
                        break;
                    }
                    nTrailOff -= nSize;
                    nSize
                        = pEUDCTab->mnTrail2End - pEUDCTab->mnTrail2Start + 1;
                    if (nTrailOff < nSize)
                    {
                        cConv |= pEUDCTab->mnTrail2Start + nTrailOff;
                        break;
                    }
                    nTrailOff -= nSize;
                    cConv |= pEUDCTab->mnTrail3Start + nTrailOff;
                    break;
                }
                pEUDCTab++;
            }

            /* FIXME
             * SB: Not sure why this is in here.  Plus, it does not work as
             * intended when (c & 0xFF) == 0, because the next !cConv check
             * will then think c has not yet been converted...
             */
            if (c >= RTL_TEXTCVT_BYTE_PRIVATE_START
                && c <= RTL_TEXTCVT_BYTE_PRIVATE_END)
            {
                if ( nFlags & RTL_UNICODETOTEXT_FLAGS_PRIVATE_MAPTO0 )
                    cConv = (sal_Char)(sal_uChar)(c & 0xFF);
            }
        }

        if ( !cConv )
        {
            if ( nFlags & RTL_UNICODETOTEXT_FLAGS_UNDEFINED_REPLACE )
            {
                /* !!! */
            }

            if ( nFlags & RTL_UNICODETOTEXT_FLAGS_UNDEFINED_REPLACESTR )
            {
                /* !!! */
            }

            /* Handle undefined and surrogates characters */
            /* (all surrogates characters are undefined) */
            if (ImplHandleUndefinedUnicodeToTextChar(pData,
                                                     &pSrcBuf,
                                                     pEndSrcBuf,
                                                     &pDestBuf,
                                                     pEndDestBuf,
                                                     nFlags,
                                                     pInfo))
                continue;
            else
                break;
        }

        /* SingleByte */
        if ( !(cConv & 0xFF00) )
        {
            if ( pDestBuf == pEndDestBuf )
            {
                *pInfo |= RTL_UNICODETOTEXT_INFO_ERROR | RTL_UNICODETOTEXT_INFO_DESTBUFFERTOSMALL;
                break;
            }

            *pDestBuf = (sal_Char)(sal_uChar)(cConv & 0xFF);
            pDestBuf++;
        }
        else
        {
            if ( pDestBuf+1 >= pEndDestBuf )
            {
                *pInfo |= RTL_UNICODETOTEXT_INFO_ERROR | RTL_UNICODETOTEXT_INFO_DESTBUFFERTOSMALL;
                break;
            }

            *pDestBuf = (sal_Char)(sal_uChar)((cConv >> 8) & 0xFF);
            pDestBuf++;
            *pDestBuf = (sal_Char)(sal_uChar)(cConv & 0xFF);
            pDestBuf++;
        }

        pSrcBuf++;
    }

    *pSrcCvtChars = nSrcChars - (pEndSrcBuf-pSrcBuf);
    return (nDestBytes - (pEndDestBuf-pDestBuf));
}

/* ======================================================================= */

#define JIS_EUC_LEAD_OFF                                        0x80
#define JIS_EUC_TRAIL_OFF                                       0x80

/* ----------------------------------------------------------------------- */

sal_Size ImplEUCJPToUnicode( const ImplTextConverterData* pData,
                             void* pContext,
                             const sal_Char* pSrcBuf, sal_Size nSrcBytes,
                             sal_Unicode* pDestBuf, sal_Size nDestChars,
                             sal_uInt32 nFlags, sal_uInt32* pInfo,
                             sal_Size* pSrcCvtBytes )
{
    sal_uChar                   c;
    sal_uChar                   cLead = '\0';
    sal_uChar                   cTrail = '\0';
    sal_Unicode                 cConv;
    const ImplDBCSToUniLeadTab* pLeadEntry;
    const ImplDBCSToUniLeadTab* pLeadTab;
    const ImplEUCJPConvertData* pConvertData = (const ImplEUCJPConvertData*)pData;
    sal_Unicode*                pEndDestBuf;
    const sal_Char*             pEndSrcBuf;

    (void) pContext; /* unused */

    *pInfo = 0;
    pEndDestBuf = pDestBuf+nDestChars;
    pEndSrcBuf  = pSrcBuf+nSrcBytes;
    while ( pSrcBuf < pEndSrcBuf )
    {
        c = (sal_uChar)*pSrcBuf;

        /* ASCII */
        if ( c <= 0x7F )
            cConv = c;
        else
        {
            /* SS2 - Half-width katakana */
            /* 8E + A1-DF */
            if ( c == 0x8E )
            {
                /* Source buffer to small */
                if ( pSrcBuf + 1 == pEndSrcBuf )
                {
                    *pInfo |= RTL_TEXTTOUNICODE_INFO_SRCBUFFERTOSMALL;
                    break;
                }

                pSrcBuf++;
                c = (sal_uChar)*pSrcBuf;
                if ( (c >= 0xA1) && (c <= 0xDF) )
                    cConv = 0xFF61+(c-0xA1);
                else
                {
                    cConv = 0;
                    cLead = 0x8E;
                    cTrail = c;
                }
            }
            else
            {
                /* SS3 - JIS 0212-1990 */
                /* 8F + A1-FE + A1-FE */
                if ( c == 0x8F )
                {
                    /* Source buffer to small */
                    if (pEndSrcBuf - pSrcBuf < 3)
                    {
                        *pInfo |= RTL_TEXTTOUNICODE_INFO_SRCBUFFERTOSMALL;
                        break;
                    }

                    pSrcBuf++;
                    cLead = (sal_uChar)*pSrcBuf;
                    pSrcBuf++;
                    cTrail = (sal_uChar)*pSrcBuf;
                    pLeadTab = pConvertData->mpJIS0212ToUniLeadTab;
                }
                /* CodeSet 2 JIS 0208-1997 */
                /* A1-FE + A1-FE */
                else
                {
                    /* Source buffer to small */
                    if ( pSrcBuf + 1 == pEndSrcBuf )
                    {
                        *pInfo |= RTL_TEXTTOUNICODE_INFO_SRCBUFFERTOSMALL;
                        break;
                    }

                    cLead = c;
                    pSrcBuf++;
                    cTrail = (sal_uChar)*pSrcBuf;
                    pLeadTab = pConvertData->mpJIS0208ToUniLeadTab;
                }

                /* Undefined Range */
                if ( (cLead < JIS_EUC_LEAD_OFF) || (cTrail < JIS_EUC_TRAIL_OFF) )
                    cConv = 0;
                else
                {
                    cLead   -= JIS_EUC_LEAD_OFF;
                    cTrail  -= JIS_EUC_TRAIL_OFF;
                    pLeadEntry = pLeadTab+cLead;
                    if ( (cTrail >= pLeadEntry->mnTrailStart) && (cTrail <= pLeadEntry->mnTrailEnd) )
                        cConv = pLeadEntry->mpToUniTrailTab[cTrail-pLeadEntry->mnTrailStart];
                    else
                        cConv = 0;
                }
            }

            if ( !cConv )
            {
                /* Wir vergleichen den kompletten Trailbereich den wir */
                /* definieren, der normalerweise groesser sein kann als */
                /* der definierte. Dies machen wir, damit Erweiterungen von */
                /* uns nicht beruecksichtigten Encodings so weit wie */
                /* moeglich auch richtig zu behandeln, das double byte */
                /* characters auch als ein einzelner Character behandelt */
                /* wird. */
                if ( (cLead < JIS_EUC_LEAD_OFF) || (cTrail < JIS_EUC_TRAIL_OFF) )
                {
                    *pInfo |= RTL_TEXTTOUNICODE_INFO_INVALID;
                    if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_INVALID_MASK) == RTL_TEXTTOUNICODE_FLAGS_INVALID_ERROR )
                    {
                        *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR;
                        break;
                    }
                    else if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_INVALID_MASK) == RTL_TEXTTOUNICODE_FLAGS_INVALID_IGNORE )
                    {
                        pSrcBuf++;
                        continue;
                    }
                    else
                        cConv = RTL_TEXTENC_UNICODE_REPLACEMENT_CHARACTER;
                }
                else
                {
                    *pInfo |= RTL_TEXTTOUNICODE_INFO_MBUNDEFINED;
                    if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_MASK) == RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_ERROR )
                    {
                        *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR;
                        break;
                    }
                    else if ( (nFlags & RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_MASK) == RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_IGNORE )
                    {
                        pSrcBuf++;
                        continue;
                    }
                    else
                        cConv = RTL_TEXTENC_UNICODE_REPLACEMENT_CHARACTER;
                }
            }
        }

        if ( pDestBuf == pEndDestBuf )
        {
            *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR | RTL_TEXTTOUNICODE_INFO_DESTBUFFERTOSMALL;
            break;
        }

        *pDestBuf = cConv;
        pDestBuf++;
        pSrcBuf++;
    }

    *pSrcCvtBytes = nSrcBytes - (pEndSrcBuf-pSrcBuf);
    return (nDestChars - (pEndDestBuf-pDestBuf));
}

/* ----------------------------------------------------------------------- */

sal_Size ImplUnicodeToEUCJP( const ImplTextConverterData* pData,
                             void* pContext,
                             const sal_Unicode* pSrcBuf, sal_Size nSrcChars,
                             sal_Char* pDestBuf, sal_Size nDestBytes,
                             sal_uInt32 nFlags, sal_uInt32* pInfo,
                             sal_Size* pSrcCvtChars )
{
    sal_uInt32                  cConv;
    sal_Unicode                 c;
    sal_uChar                   nHighChar;
    sal_uChar                   nLowChar;
    const ImplUniToDBCSHighTab* pHighEntry;
    const ImplUniToDBCSHighTab* pHighTab;
    const ImplEUCJPConvertData* pConvertData = (const ImplEUCJPConvertData*)pData;
    sal_Char*                   pEndDestBuf;
    const sal_Unicode*          pEndSrcBuf;

    (void) pContext; /* unused */

    *pInfo = 0;
    pEndDestBuf = pDestBuf+nDestBytes;
    pEndSrcBuf  = pSrcBuf+nSrcChars;
    while ( pSrcBuf < pEndSrcBuf )
    {
        c = *pSrcBuf;

        /* ASCII */
        if ( c <= 0x7F )
            cConv = c;
        /* Half-width katakana */
        else if ( (c >= 0xFF61) && (c <= 0xFF9F) )
            cConv = 0x8E00+0xA1+(c-0xFF61);
        else
        {
            nHighChar = (sal_uChar)((c >> 8) & 0xFF);
            nLowChar = (sal_uChar)(c & 0xFF);

            /* JIS 0208 */
            pHighTab = pConvertData->mpUniToJIS0208HighTab;
            pHighEntry = pHighTab+nHighChar;
            if ( (nLowChar >= pHighEntry->mnLowStart) && (nLowChar <= pHighEntry->mnLowEnd) )
            {
                cConv = pHighEntry->mpToUniTrailTab[nLowChar-pHighEntry->mnLowStart];
                if (cConv != 0)
                    cConv |= 0x8080;
            }
            else
                cConv = 0;

            /* JIS 0212 */
            if ( !cConv )
            {
                pHighTab = pConvertData->mpUniToJIS0212HighTab;
                pHighEntry = pHighTab+nHighChar;
                if ( (nLowChar >= pHighEntry->mnLowStart) && (nLowChar <= pHighEntry->mnLowEnd) )
                {
                    cConv = pHighEntry->mpToUniTrailTab[nLowChar-pHighEntry->mnLowStart];
                    if (cConv != 0)
                        cConv |= 0x8F8080;
                }

                if ( !cConv )
                {
                    if ( nFlags & RTL_UNICODETOTEXT_FLAGS_UNDEFINED_REPLACE )
                    {
                        /* !!! */
                    }

                    if ( nFlags & RTL_UNICODETOTEXT_FLAGS_UNDEFINED_REPLACESTR )
                    {
                        /* !!! */
                    }

                    /* Handle undefined and surrogates characters */
                    /* (all surrogates characters are undefined) */
                    if (ImplHandleUndefinedUnicodeToTextChar(pData,
                                                             &pSrcBuf,
                                                             pEndSrcBuf,
                                                             &pDestBuf,
                                                             pEndDestBuf,
                                                             nFlags,
                                                             pInfo))
                        continue;
                    else
                        break;
                }
            }
        }

        /* SingleByte */
        if ( !(cConv & 0xFFFF00) )
        {
            if ( pDestBuf == pEndDestBuf )
            {
                *pInfo |= RTL_UNICODETOTEXT_INFO_ERROR | RTL_UNICODETOTEXT_INFO_DESTBUFFERTOSMALL;
                break;
            }

            *pDestBuf = (sal_Char)(sal_uChar)(cConv & 0xFF);
            pDestBuf++;
        }
        /* DoubleByte */
        else if ( !(cConv & 0xFF0000) )
        {
            if ( pDestBuf+1 >= pEndDestBuf )
            {
                *pInfo |= RTL_UNICODETOTEXT_INFO_ERROR | RTL_UNICODETOTEXT_INFO_DESTBUFFERTOSMALL;
                break;
            }

            *pDestBuf = (sal_Char)(sal_uChar)((cConv >> 8) & 0xFF);
            pDestBuf++;
            *pDestBuf = (sal_Char)(sal_uChar)(cConv & 0xFF);
            pDestBuf++;
        }
        else
        {
            if ( pDestBuf+2 >= pEndDestBuf )
            {
                *pInfo |= RTL_UNICODETOTEXT_INFO_ERROR | RTL_UNICODETOTEXT_INFO_DESTBUFFERTOSMALL;
                break;
            }

            *pDestBuf = (sal_Char)(sal_uChar)((cConv >> 16) & 0xFF);
            pDestBuf++;
            *pDestBuf = (sal_Char)(sal_uChar)((cConv >> 8) & 0xFF);
            pDestBuf++;
            *pDestBuf = (sal_Char)(sal_uChar)(cConv & 0xFF);
            pDestBuf++;
        }

        pSrcBuf++;
    }

    *pSrcCvtChars = nSrcChars - (pEndSrcBuf-pSrcBuf);
    return (nDestBytes - (pEndDestBuf-pDestBuf));
}
