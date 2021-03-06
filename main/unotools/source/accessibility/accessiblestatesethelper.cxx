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
#include "precompiled_unotools.hxx"


#include "unotools/accessiblestatesethelper.hxx"
#include <rtl/uuid.h>
#include <tools/debug.hxx>

#if 0
#include <bitset>
#endif

// defines how many states the bitfield can contain
// it has the size of 64 because I use a uInt64
#define BITFIELDSIZE 64

using namespace	::utl;
using namespace	::rtl;
using namespace	::com::sun::star;
using namespace	::com::sun::star::accessibility;

class AccessibleStateSetHelperImpl
{
public:
	AccessibleStateSetHelperImpl();
	AccessibleStateSetHelperImpl(const AccessibleStateSetHelperImpl& rImpl);
	~AccessibleStateSetHelperImpl();

	sal_Bool IsEmpty ()
		throw (uno::RuntimeException);
	sal_Bool Contains (sal_Int16 aState)
		throw (uno::RuntimeException);
    uno::Sequence<sal_Int16> GetStates()
        throw (uno::RuntimeException);
	void AddState(sal_Int16 aState)
		throw (uno::RuntimeException);
	void RemoveState(sal_Int16 aState)
		throw (uno::RuntimeException);
	sal_Bool Compare(const AccessibleStateSetHelperImpl* pComparativeValue,
						AccessibleStateSetHelperImpl* pOldStates,
						AccessibleStateSetHelperImpl* pNewStates)
		throw (uno::RuntimeException);

	inline void	AddStates( const sal_Int64 _nStates ) SAL_THROW( ( ) );

private:
#if 0
	::std::bitset<BITFIELDSIZE> maStates; //Bitfield
#endif
	sal_uInt64 maStates;
};

AccessibleStateSetHelperImpl::AccessibleStateSetHelperImpl()
	: maStates(0)
{
}

AccessibleStateSetHelperImpl::AccessibleStateSetHelperImpl(const AccessibleStateSetHelperImpl& rImpl)
	: maStates(rImpl.maStates)
{
}

AccessibleStateSetHelperImpl::~AccessibleStateSetHelperImpl()
{
}

inline sal_Bool AccessibleStateSetHelperImpl::IsEmpty ()
	throw (uno::RuntimeException)
{
#if 0
	return maStates.none();
#endif
	return maStates == 0;
}

inline sal_Bool AccessibleStateSetHelperImpl::Contains (sal_Int16 aState)
	throw (uno::RuntimeException)
{
	DBG_ASSERT(aState < BITFIELDSIZE, "the statesset is too small");
#if 0
	return maStates.test(aState);
#endif
	sal_uInt64 aTempBitSet(1);
	aTempBitSet <<= aState;
	return ((aTempBitSet & maStates) != 0);
}

inline uno::Sequence<sal_Int16> AccessibleStateSetHelperImpl::GetStates()
    throw (uno::RuntimeException)
{
    uno::Sequence<sal_Int16> aRet(BITFIELDSIZE);
    sal_Int16* pSeq = aRet.getArray();
    sal_Int16 nStateCount(0);
    for (sal_Int16 i = 0; i < BITFIELDSIZE; ++i)
        if (Contains(i))
        {
            *pSeq = i;
            ++pSeq;
            ++nStateCount;
        }
    aRet.realloc(nStateCount);
    return aRet;
}

inline void AccessibleStateSetHelperImpl::AddStates( const sal_Int64 _nStates ) SAL_THROW( ( ) )
{
	maStates |= _nStates;
}

inline void AccessibleStateSetHelperImpl::AddState(sal_Int16 aState)
	throw (uno::RuntimeException)
{
	DBG_ASSERT(aState < BITFIELDSIZE, "the statesset is too small");
#if 0
	maStates.set(aState);
#endif
	sal_uInt64 aTempBitSet(1);
	aTempBitSet <<= aState;
	maStates |= aTempBitSet;
}

inline void AccessibleStateSetHelperImpl::RemoveState(sal_Int16 aState)
	throw (uno::RuntimeException)
{
	DBG_ASSERT(aState < BITFIELDSIZE, "the statesset is too small");
#if 0
	maStates.set(aState, 0);
#endif
	sal_uInt64 aTempBitSet(1);
	aTempBitSet <<= aState;
	aTempBitSet = ~aTempBitSet;
	maStates &= aTempBitSet;
}

inline sal_Bool AccessibleStateSetHelperImpl::Compare(
	const AccessibleStateSetHelperImpl* pComparativeValue,
		AccessibleStateSetHelperImpl* pOldStates,
		AccessibleStateSetHelperImpl* pNewStates)
	throw (uno::RuntimeException)
{
	sal_Bool bResult(sal_False);
	if (pComparativeValue && pOldStates && pNewStates)
	{
		if (maStates == pComparativeValue->maStates)
			bResult = sal_True;
		else
		{
#if 0
			std::bitset<BITFIELDSIZE> aTempBitSet(maStates);
#endif
			sal_uInt64 aTempBitSet(maStates);
			aTempBitSet ^= pComparativeValue->maStates;
			pOldStates->maStates = aTempBitSet;
			pOldStates->maStates &= maStates;
			pNewStates->maStates = aTempBitSet;
			pNewStates->maStates &= pComparativeValue->maStates;
		}
	}
	return bResult;
}


//=====  internal  ============================================================

AccessibleStateSetHelper::AccessibleStateSetHelper ()
	: mpHelperImpl(NULL)
{
	mpHelperImpl = new AccessibleStateSetHelperImpl();
}

AccessibleStateSetHelper::AccessibleStateSetHelper ( const sal_Int64 _nInitialStates )
	: mpHelperImpl(NULL)
{
	mpHelperImpl = new AccessibleStateSetHelperImpl();
	mpHelperImpl->AddStates( _nInitialStates );
}

AccessibleStateSetHelper::AccessibleStateSetHelper (const AccessibleStateSetHelper& rHelper)
	: cppu::WeakImplHelper1<XAccessibleStateSet>()
    , mpHelperImpl(NULL)
{
	if (rHelper.mpHelperImpl)
		mpHelperImpl = new AccessibleStateSetHelperImpl(*rHelper.mpHelperImpl);
	else
		mpHelperImpl = new AccessibleStateSetHelperImpl();
}

AccessibleStateSetHelper::~AccessibleStateSetHelper(void)
{
	delete mpHelperImpl;
}

//=====  XAccessibleStateSet  ==============================================

    /** Checks whether the current state set is empty.

        @return
            Returns <TRUE/> if there is no state in this state set and
            <FALSE/> if there is at least one state set in it.
    */
sal_Bool SAL_CALL AccessibleStateSetHelper::isEmpty ()
	throw (uno::RuntimeException)
{
    ::vos::OGuard aGuard (maMutex);
	return mpHelperImpl->IsEmpty();
}

    /** Checks if the given state is a member of the state set of this
        object.

        @param aState
            The state for which to check membership.  This has to be one of
            the constants of <type>AccessibleStateType</type>.

        @return
            Returns <TRUE/> if the given state is a member of this object's
            state set and <FALSE/> otherwise.
    */
sal_Bool SAL_CALL AccessibleStateSetHelper::contains (sal_Int16 aState)
	throw (uno::RuntimeException)
{
    ::vos::OGuard aGuard (maMutex);
	return mpHelperImpl->Contains(aState);
}

    /** Checks if all of the given states are in this object's state
        set.

        @param aStateSet
            This sequence of states is interpreted as set and every of its
            members, duplicates are ignored, is checked for membership in
            this object's state set.  Each state has to be one of the
            constants of <type>AccessibleStateType</type>.

        @return
            Returns <TRUE/> if all states of the given state set are members
            of this object's state set.  <FALSE/> is returned if at least
            one of the states in the given state is not a member of this
            object's state set.
    */
sal_Bool SAL_CALL AccessibleStateSetHelper::containsAll
	(const uno::Sequence<sal_Int16>& rStateSet)
	throw (uno::RuntimeException)
{
    ::vos::OGuard aGuard (maMutex);
	sal_Int32 nCount(rStateSet.getLength());
	const sal_Int16* pStates = rStateSet.getConstArray();
	sal_Int32 i = 0;
	sal_Bool bFound(sal_True);
	while (i < nCount)
	{
		bFound = mpHelperImpl->Contains(pStates[i]);
		i++;
	}
	return bFound;
}

uno::Sequence<sal_Int16> SAL_CALL AccessibleStateSetHelper::getStates()
    throw (uno::RuntimeException)
{
    ::vos::OGuard aGuard(maMutex);
    return mpHelperImpl->GetStates();
}

void AccessibleStateSetHelper::AddState(sal_Int16 aState)
	throw (uno::RuntimeException)
{
    ::vos::OGuard aGuard (maMutex);
	mpHelperImpl->AddState(aState);
}

void AccessibleStateSetHelper::RemoveState(sal_Int16 aState)
	throw (uno::RuntimeException)
{
    ::vos::OGuard aGuard (maMutex);
	mpHelperImpl->RemoveState(aState);
}

sal_Bool AccessibleStateSetHelper::Compare(
	const AccessibleStateSetHelper& rComparativeValue,
		AccessibleStateSetHelper& rOldStates,
		AccessibleStateSetHelper& rNewStates)
	throw (uno::RuntimeException)
{
    ::vos::OGuard aGuard (maMutex);
	return mpHelperImpl->Compare(rComparativeValue.mpHelperImpl,
		rOldStates.mpHelperImpl, rNewStates.mpHelperImpl);
}

//=====  XTypeProvider  =======================================================

uno::Sequence< ::com::sun::star::uno::Type>
	AccessibleStateSetHelper::getTypes (void)
    throw (::com::sun::star::uno::RuntimeException)
{
	const ::com::sun::star::uno::Type aTypeList[] = {
    	::getCppuType((const uno::Reference<
        	XAccessibleStateSet>*)0),
    	::getCppuType((const uno::Reference<
        	lang::XTypeProvider>*)0)
		};
	::com::sun::star::uno::Sequence< ::com::sun::star::uno::Type>
    	aTypeSequence (aTypeList, 2);
	return aTypeSequence;
}

uno::Sequence<sal_Int8> SAL_CALL
	AccessibleStateSetHelper::getImplementationId (void)
    throw (::com::sun::star::uno::RuntimeException)
{
    ::vos::OGuard aGuard (maMutex);
	static uno::Sequence<sal_Int8> aId;
	if (aId.getLength() == 0)
	{
		aId.realloc (16);
		rtl_createUuid ((sal_uInt8 *)aId.getArray(), 0, sal_True);
	}
	return aId;
}
