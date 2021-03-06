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



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_basctl.hxx"


#include <ide_pch.hxx>

#define _SI_NOSBXCONTROLS

#include <basic/sbx.hxx>
#include <bastype2.hxx>
#include <basobj.hxx>
#include <baside2.hrc>
#include <iderid.hxx>
#include <bastypes.hxx>
#include <basdoc.hxx>
#include <com/sun/star/script/XLibraryContainer.hpp>
#include <com/sun/star/script/XLibraryContainerPassword.hpp>
#include <deque>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star;


typedef std::deque< SvLBoxEntry* > EntryArray;


void __EXPORT BasicTreeListBox::RequestingChilds( SvLBoxEntry* pEntry )
{
    BasicEntryDescriptor aDesc( GetEntryDescriptor( pEntry ) );
    ScriptDocument aDocument( aDesc.GetDocument() );
    OSL_ENSURE( aDocument.isAlive(), "BasicTreeListBox::RequestingChilds: invalid document!" );
    if ( !aDocument.isAlive() )
        return;

    LibraryLocation eLocation( aDesc.GetLocation() );
    BasicEntryType eType( aDesc.GetType() );    

	if ( eType == OBJ_TYPE_DOCUMENT )
    {
        ImpCreateLibEntries( pEntry, aDocument, eLocation );
    }
	else if ( eType == OBJ_TYPE_LIBRARY )
	{
        String aLibName( aDesc.GetLibName() );
        ::rtl::OUString aOULibName( aLibName );

        // check password
		sal_Bool bOK = sal_True;
        Reference< script::XLibraryContainer > xModLibContainer( aDocument.getLibraryContainer( E_SCRIPTS ) );
        if ( xModLibContainer.is() && xModLibContainer->hasByName( aOULibName ) )
        {
            Reference< script::XLibraryContainerPassword > xPasswd( xModLibContainer, UNO_QUERY );
            if ( xPasswd.is() && xPasswd->isLibraryPasswordProtected( aOULibName ) && !xPasswd->isLibraryPasswordVerified( aOULibName ) )
            {
                String aPassword;
			    bOK = QueryPassword( xModLibContainer, aLibName, aPassword );
            }
        }

        if ( bOK )
        {
            // load module library              
            sal_Bool bModLibLoaded = sal_False;            
            if ( xModLibContainer.is() && xModLibContainer->hasByName( aOULibName ) )
            {
                if ( !xModLibContainer->isLibraryLoaded( aOULibName ) )
		        {
                    EnterWait();
                    xModLibContainer->loadLibrary( aOULibName );
                    LeaveWait();
                }
                bModLibLoaded = xModLibContainer->isLibraryLoaded( aOULibName );
            }

            // load dialog library
            sal_Bool bDlgLibLoaded = sal_False;
            Reference< script::XLibraryContainer > xDlgLibContainer( aDocument.getLibraryContainer( E_DIALOGS ), UNO_QUERY );           
            if ( xDlgLibContainer.is() && xDlgLibContainer->hasByName( aOULibName ) )
            {
                if ( !xDlgLibContainer->isLibraryLoaded( aOULibName ) )
		        {
                    EnterWait();
			        xDlgLibContainer->loadLibrary( aOULibName );
                    LeaveWait();
                }
                bDlgLibLoaded = xDlgLibContainer->isLibraryLoaded( aOULibName );
            }

            if ( bModLibLoaded || bDlgLibLoaded )
            {
                // create the sub entries
                ImpCreateLibSubEntries( pEntry, aDocument, aLibName );

			    // exchange image
                bool bDlgMode = ( nMode & BROWSEMODE_DIALOGS ) && !( nMode & BROWSEMODE_MODULES );
                Image aImage( IDEResId( bDlgMode ? RID_IMG_DLGLIB : RID_IMG_MODLIB ) );
                Image aImageHC( IDEResId( bDlgMode ? RID_IMG_DLGLIB_HC : RID_IMG_MODLIB_HC ) );
                SetEntryBitmaps( pEntry, aImage, aImageHC );
		    }
		    else
		    {
                DBG_ERROR( "BasicTreeListBox::RequestingChilds: Error loading library!" );
		    }
        }
    }
    else if ( eType == OBJ_TYPE_DOCUMENT_OBJECTS 
            || eType == OBJ_TYPE_USERFORMS
            || eType == OBJ_TYPE_NORMAL_MODULES
            || eType == OBJ_TYPE_CLASS_MODULES )
    {
        String aLibName( aDesc.GetLibName() );
        ImpCreateLibSubSubEntriesInVBAMode( pEntry, aDocument, aLibName );
    }
	else {
		DBG_ERROR( "BasicTreeListBox::RequestingChilds: Unknown Type!" );
    }
}

void __EXPORT BasicTreeListBox::ExpandedHdl()
{
	SvLBoxEntry* pEntry = GetHdlEntry();
	DBG_ASSERT( pEntry, "Was wurde zugeklappt?" );
	// Die OnDemand erzeugten Childs loeschen,
	// SubChilds werden automatisch geloescht.
	if ( !IsExpanded( pEntry ) && pEntry->HasChildsOnDemand() )
	{
		SvLBoxEntry* pChild = FirstChild( pEntry );
		while ( pChild )
		{
			GetModel()->Remove( pChild );	// Ruft auch den DTOR
			pChild = FirstChild( pEntry );
		}
	}
}

void BasicTreeListBox::ScanAllEntries()
{
    ScanEntry( ScriptDocument::getApplicationScriptDocument(), LIBRARY_LOCATION_USER );
    ScanEntry( ScriptDocument::getApplicationScriptDocument(), LIBRARY_LOCATION_SHARE );

    ScriptDocuments aDocuments( ScriptDocument::getAllScriptDocuments( ScriptDocument::DocumentsSorted ) );
    for (   ScriptDocuments::const_iterator doc = aDocuments.begin();
            doc != aDocuments.end();
            ++doc
        )
    {
        if ( doc->isAlive() )
            ScanEntry( *doc, LIBRARY_LOCATION_DOCUMENT );
    }
}

SbxVariable* BasicTreeListBox::FindVariable( SvLBoxEntry* pEntry )
{
	if ( !pEntry )
		return 0;

	String aLib, aModOrObj, aSubOrPropOrSObj, aPropOrSubInSObj;
    ScriptDocument aDocument( ScriptDocument::getApplicationScriptDocument() );
	EntryArray aEntries;

    while ( pEntry )
    {
        sal_uInt16 nDepth = GetModel()->GetDepth( pEntry );
        switch ( nDepth )
        {
            case 4:
            case 3:
            case 2:
            case 1: 
            {
                aEntries.push_front( pEntry );
            }
            break;
            case 0: 
            {
                aDocument = ((BasicDocumentEntry*)pEntry->GetUserData())->GetDocument();
            }
            break;
        }
        pEntry = GetParent( pEntry );
    }

	SbxVariable* pVar = 0;
    bool bDocumentObjects = false;
	if ( !aEntries.empty() )
	{
		for ( size_t n = 0; n < aEntries.size(); n++ )
		{
			SvLBoxEntry* pLE = aEntries[n];
			DBG_ASSERT( pLE, "Can not find entry in array" );
			BasicEntry* pBE = (BasicEntry*)pLE->GetUserData();
			DBG_ASSERT( pBE, "The data in the entry not found!" );
			String aName( GetEntryText( pLE ) );

			switch ( pBE->GetType() )
			{
				case OBJ_TYPE_LIBRARY:
				{
                    BasicManager* pBasMgr = aDocument.getBasicManager();
                    if ( pBasMgr )
                        pVar = pBasMgr->GetLib( aName );
                }
				break;
				case OBJ_TYPE_MODULE:
				{
					DBG_ASSERT( pVar && pVar->IsA( TYPE(StarBASIC) ), "FindVariable: Ungueltiges Basic" );
                    // extract the module name from the string like "Sheet1 (Example1)"
                    if( bDocumentObjects )
                    {
                        sal_uInt16 nIndex = 0;
                        aName = aName.GetToken( 0, ' ', nIndex );
                    }
					pVar = ((StarBASIC*)pVar)->FindModule( aName );
				}
				break;
				case OBJ_TYPE_METHOD:
				{
					DBG_ASSERT( pVar && ( (pVar->IsA( TYPE(SbModule) )) || (pVar->IsA( TYPE(SbxObject) )) ), "FindVariable: Ungueltiges Modul/Objekt" );
					pVar = ((SbxObject*)pVar)->GetMethods()->Find( aName, SbxCLASS_METHOD );
				}
				break;
				case OBJ_TYPE_DIALOG:
				{
					// sbx dialogs removed
				}
				break;
                case OBJ_TYPE_DOCUMENT_OBJECTS:
                    bDocumentObjects = true;
                case OBJ_TYPE_USERFORMS:
                case OBJ_TYPE_NORMAL_MODULES:
                case OBJ_TYPE_CLASS_MODULES:
                {
                    // skip, to find the child entry.
                    continue;
                }
                default:
                {
                    DBG_ERROR( "FindVariable: Unbekannter Typ!" );
                    pVar = 0;
                }
                break;
			}
			if ( !pVar )
				break;
		}
	}

	return pVar;
}

BasicEntryDescriptor BasicTreeListBox::GetEntryDescriptor( SvLBoxEntry* pEntry )
{
    ScriptDocument aDocument( ScriptDocument::getApplicationScriptDocument() );
    LibraryLocation eLocation = LIBRARY_LOCATION_UNKNOWN;
	String aLibName;
	String aLibSubName;
	String aName;
	String aMethodName;
    BasicEntryType eType = OBJ_TYPE_UNKNOWN;

	if ( !pEntry )
        return BasicEntryDescriptor( aDocument, eLocation, aLibName, aLibSubName, aName, aMethodName, eType );

	EntryArray aEntries;

    while ( pEntry )
    {
        sal_uInt16 nDepth = GetModel()->GetDepth( pEntry );
        switch ( nDepth )
        {
            case 4:
            case 3:
            case 2:
            case 1: 
            {
                aEntries.push_front( pEntry );
            }
            break;
            case 0: 
            {
                BasicDocumentEntry* pBasicDocumentEntry = (BasicDocumentEntry*)pEntry->GetUserData();
                if ( pBasicDocumentEntry )
                {
                    aDocument = pBasicDocumentEntry->GetDocument();
                    eLocation = pBasicDocumentEntry->GetLocation();
                    eType = OBJ_TYPE_DOCUMENT;
                }
            }
            break;
        }
        pEntry = GetParent( pEntry );
    }

	if ( !aEntries.empty() )
	{
		for ( size_t n = 0; n < aEntries.size(); n++ )
		{
			SvLBoxEntry* pLE = aEntries[n];
			DBG_ASSERT( pLE, "Entrie im Array nicht gefunden" );
			BasicEntry* pBE = (BasicEntry*)pLE->GetUserData();
			DBG_ASSERT( pBE, "Keine Daten im Eintrag gefunden!" );

			switch ( pBE->GetType() )
			{
				case OBJ_TYPE_LIBRARY:
				{
					aLibName = GetEntryText( pLE );
                    eType = pBE->GetType();
				}
				break;
				case OBJ_TYPE_MODULE:
				{
					aName = GetEntryText( pLE );
                    eType = pBE->GetType();
				}
				break;
				case OBJ_TYPE_METHOD:
				{
					aMethodName = GetEntryText( pLE );
                    eType = pBE->GetType();
				}
				break;
				case OBJ_TYPE_DIALOG:
				{
					aName = GetEntryText( pLE );
					eType = pBE->GetType();
				}
				break;
                case OBJ_TYPE_DOCUMENT_OBJECTS:
                case OBJ_TYPE_USERFORMS:
                case OBJ_TYPE_NORMAL_MODULES:
                case OBJ_TYPE_CLASS_MODULES:
				{
					aLibSubName = GetEntryText( pLE );
					eType = pBE->GetType();
				}
				break;
				default:
                {
                    DBG_ERROR( "GetEntryDescriptor: Unbekannter Typ!" );
                    eType = OBJ_TYPE_UNKNOWN;
                }
                break;
			}

			if ( eType == OBJ_TYPE_UNKNOWN )
				break;
		}
	}

    return BasicEntryDescriptor( aDocument, eLocation, aLibName, aLibSubName, aName, aMethodName, eType );
}

sal_uInt16 BasicTreeListBox::ConvertType( BasicEntryType eType )
{
    sal_uInt16 nType = OBJ_TYPE_UNKNOWN;

    switch ( eType )
    {
        case OBJ_TYPE_DOCUMENT:
        {
            nType = BASICIDE_TYPE_SHELL;
        }
        break;
        case OBJ_TYPE_LIBRARY:
        {
            nType = BASICIDE_TYPE_LIBRARY;
        }
        break;
        case OBJ_TYPE_MODULE:
        {
            nType = BASICIDE_TYPE_MODULE;
        }
        break;
        case OBJ_TYPE_DIALOG:
        {
            nType = BASICIDE_TYPE_DIALOG;
        }
        break;
        case OBJ_TYPE_METHOD:
        {
            nType = BASICIDE_TYPE_METHOD;
        }
        break;
		default: ;
    }

    return nType;
}

bool BasicTreeListBox::IsValidEntry( SvLBoxEntry* pEntry )
{
    bool bIsValid = false;

    BasicEntryDescriptor aDesc( GetEntryDescriptor( pEntry ) );
    ScriptDocument aDocument( aDesc.GetDocument() );
    LibraryLocation eLocation( aDesc.GetLocation() );
    String aLibName( aDesc.GetLibName() );
    String aName( aDesc.GetName() );
    String aMethodName( aDesc.GetMethodName() );
    BasicEntryType eType( aDesc.GetType() );

    switch ( eType )
    {
	    case OBJ_TYPE_DOCUMENT:
	    {
            bIsValid =  aDocument.isAlive()
                    &&  (   aDocument.isApplication()
                        ||  GetRootEntryName( aDocument, eLocation ) == GetEntryText( pEntry )
                        );
	    }
	    break;
        case OBJ_TYPE_LIBRARY:
        {
            bIsValid = aDocument.hasLibrary( E_SCRIPTS, aLibName ) || aDocument.hasLibrary( E_DIALOGS, aLibName );
        }
        break;
        case OBJ_TYPE_MODULE:
        {
            bIsValid = aDocument.hasModule( aLibName, aName );
        }
        break;
        case OBJ_TYPE_DIALOG:
        {
            bIsValid = aDocument.hasDialog( aLibName, aName );
        }
        break;
        case OBJ_TYPE_METHOD:
        {
            bIsValid = BasicIDE::HasMethod( aDocument, aLibName, aName, aMethodName );
        }
        break;
        case OBJ_TYPE_DOCUMENT_OBJECTS:
        case OBJ_TYPE_USERFORMS:
        case OBJ_TYPE_NORMAL_MODULES:
        case OBJ_TYPE_CLASS_MODULES:
        {
            bIsValid = true;
        }
        break;
		default: ;
    }

    return bIsValid;
}

SbModule* BasicTreeListBox::FindModule( SvLBoxEntry* pEntry )
{
	SbxVariable* pVar = FindVariable( pEntry );
	if ( pVar && pVar->IsA( TYPE(SbModule ) ) )
		return (SbModule*)pVar;
	return 0;
}

SvLBoxEntry* BasicTreeListBox::FindRootEntry( const ScriptDocument& rDocument, LibraryLocation eLocation )
{
    OSL_ENSURE( rDocument.isValid(), "BasicTreeListBox::FindRootEntry: invalid document!" );
    sal_uLong nRootPos = 0;
    SvLBoxEntry* pRootEntry = GetEntry( nRootPos );
    while ( pRootEntry )
    {
        DBG_ASSERT( (((BasicEntry*)pRootEntry->GetUserData())->GetType() == OBJ_TYPE_DOCUMENT ), "Kein Shelleintrag?" );
        BasicDocumentEntry* pBasicDocumentEntry = (BasicDocumentEntry*)pRootEntry->GetUserData();
        if ( pBasicDocumentEntry && ( pBasicDocumentEntry->GetDocument() == rDocument ) && pBasicDocumentEntry->GetLocation() == eLocation )
            return pRootEntry;
        pRootEntry = GetEntry( ++nRootPos );
    }
    return 0;
}

String CreateMgrAndLibStr( const String& rMgrName, const String& rLibName )
{
	String aName( '[' );
	aName += rMgrName;
	aName += String( RTL_CONSTASCII_USTRINGPARAM( "]." ) );
	aName += rLibName;
	return aName;
}
