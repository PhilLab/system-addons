/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsComboboxControlFrame.h"
#include "nsIFrameManager.h"
#include "nsFormFrame.h"
#include "nsFormControlFrame.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMElement.h"
#include "nsIListControlFrame.h"
#include "nsIDOMHTMLCollection.h" 
#include "nsIDOMHTMLSelectElement.h" 
#include "nsIDOMHTMLOptionElement.h" 
#include "nsIDOMNSHTMLOptionCollection.h" 
#include "nsIPresShell.h"
#include "nsIPresState.h"
#include "nsISupportsArray.h"
#include "nsIDeviceContext.h"
#include "nsIView.h"
#include "nsIScrollableView.h"
#include "nsIEventStateManager.h"
#include "nsIDOMNode.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIStatefulFrame.h"
#include "nsISupportsArray.h"
#include "nsISelectControlFrame.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsIDOMMouseListener.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDocument.h"

#ifdef DO_NEW_REFLOW
#include "nsIFontMetrics.h"
#endif


static NS_DEFINE_IID(kIFormControlFrameIID,      NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kIComboboxControlFrameIID,  NS_ICOMBOBOXCONTROLFRAME_IID);
static NS_DEFINE_IID(kIListControlFrameIID,      NS_ILISTCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID,      NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIFrameIID,                 NS_IFRAME_IID);
static NS_DEFINE_IID(kIAnonymousContentCreatorIID, NS_IANONYMOUS_CONTENT_CREATOR_IID);
static NS_DEFINE_IID(kIDOMNodeIID,               NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID,       NS_IPRIVATEDOMEVENT_IID);

// Drop down list event management.
// The combo box uses the following strategy for managing the drop-down list.
// If the combo box or it's arrow button is clicked on the drop-down list is displayed
// If mouse exit's the combo box with the drop-down list displayed the drop-down list
// is asked to capture events
// The drop-down list will capture all events including mouse down and up and will always
// return with ListWasSelected method call regardless of whether an item in the list was
// actually selected.
// The ListWasSelected code will turn off mouse-capture for the drop-down list.
// The drop-down list does not explicitly set capture when it is in the drop-down mode.


//XXX: This is temporary. It simulates psuedo states by using a attribute selector on 

const char * kMozDropdownActive = "-moz-dropdown-active";

const PRInt32 kSizeNotSet = -1;

nsresult
NS_NewComboboxControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aStateFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsComboboxControlFrame* it = new (aPresShell) nsComboboxControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // set the state flags (if any are provided)
  nsFrameState state;
  it->GetFrameState( &state );
  state |= aStateFlags;
  it->SetFrameState( state );
  *aNewFrame = it;
  return NS_OK;
}

//-----------------------------------------------------------
// Reflow Debugging Macros
// These let us "see" how many reflow counts are happening
//-----------------------------------------------------------
#ifdef DO_REFLOW_COUNTER

#define MAX_REFLOW_CNT 1024
static PRInt32 gTotalReqs    = 0;;
static PRInt32 gTotalReflows = 0;;
static PRInt32 gReflowControlCntRQ[MAX_REFLOW_CNT];
static PRInt32 gReflowControlCnt[MAX_REFLOW_CNT];
static PRInt32 gReflowInx = -1;

#define REFLOW_COUNTER() \
  if (mReflowId > -1) \
    gReflowControlCnt[mReflowId]++;

#define REFLOW_COUNTER_REQUEST() \
  if (mReflowId > -1) \
    gReflowControlCntRQ[mReflowId]++;

#define REFLOW_COUNTER_DUMP(__desc) \
  if (mReflowId > -1) {\
    gTotalReqs    += gReflowControlCntRQ[mReflowId];\
    gTotalReflows += gReflowControlCnt[mReflowId];\
    printf("** Id:%5d %s RF: %d RQ: %d   %d/%d  %5.2f\n", \
           mReflowId, (__desc), \
           gReflowControlCnt[mReflowId], \
           gReflowControlCntRQ[mReflowId],\
           gTotalReflows, gTotalReqs, float(gTotalReflows)/float(gTotalReqs)*100.0f);\
  }

#define REFLOW_COUNTER_INIT() \
  if (gReflowInx < MAX_REFLOW_CNT) { \
    gReflowInx++; \
    mReflowId = gReflowInx; \
    gReflowControlCnt[mReflowId] = 0; \
    gReflowControlCntRQ[mReflowId] = 0; \
  } else { \
    mReflowId = -1; \
  }

// reflow messages
#define REFLOW_DEBUG_MSG(_msg1) printf((_msg1))
#define REFLOW_DEBUG_MSG2(_msg1, _msg2) printf((_msg1), (_msg2))
#define REFLOW_DEBUG_MSG3(_msg1, _msg2, _msg3) printf((_msg1), (_msg2), (_msg3))
#define REFLOW_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) printf((_msg1), (_msg2), (_msg3), (_msg4))

#else //-------------

#define REFLOW_COUNTER_REQUEST() 
#define REFLOW_COUNTER() 
#define REFLOW_COUNTER_DUMP(__desc) 
#define REFLOW_COUNTER_INIT() 

#define REFLOW_DEBUG_MSG(_msg) 
#define REFLOW_DEBUG_MSG2(_msg1, _msg2) 
#define REFLOW_DEBUG_MSG3(_msg1, _msg2, _msg3) 
#define REFLOW_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) 


#endif

//------------------------------------------
// This is for being VERY noisy
//------------------------------------------
#ifdef DO_VERY_NOISY
#define REFLOW_NOISY_MSG(_msg1) printf((_msg1))
#define REFLOW_NOISY_MSG2(_msg1, _msg2) printf((_msg1), (_msg2))
#define REFLOW_NOISY_MSG3(_msg1, _msg2, _msg3) printf((_msg1), (_msg2), (_msg3))
#define REFLOW_NOISY_MSG4(_msg1, _msg2, _msg3, _msg4) printf((_msg1), (_msg2), (_msg3), (_msg4))
#else
#define REFLOW_NOISY_MSG(_msg) 
#define REFLOW_NOISY_MSG2(_msg1, _msg2) 
#define REFLOW_NOISY_MSG3(_msg1, _msg2, _msg3) 
#define REFLOW_NOISY_MSG4(_msg1, _msg2, _msg3, _msg4) 
#endif

//------------------------------------------
// Displays value in pixels or twips
//------------------------------------------
#ifdef DO_PIXELS
#define PX(__v) __v / 15
#else
#define PX(__v) __v 
#endif

//------------------------------------------
// Asserts if we return a desired size that 
// doesn't correctly match the mComputedWidth
//------------------------------------------
#ifdef DO_UNCONSTRAINED_CHECK
#define UNCONSTRAINED_CHECK() \
if (aReflowState.mComputedWidth != NS_UNCONSTRAINEDSIZE) { \
  nscoord width = aDesiredSize.width - borderPadding.left - borderPadding.right; \
  if (width != aReflowState.mComputedWidth) { \
    printf("aDesiredSize.width %d %d != aReflowState.mComputedWidth %d\n", aDesiredSize.width, width, aReflowState.mComputedWidth); \
  } \
  NS_ASSERTION(width == aReflowState.mComputedWidth, "Returning bad value when constrained!"); \
}
#else
#define UNCONSTRAINED_CHECK()
#endif
//------------------------------------------------------
//-- Done with macros
//------------------------------------------------------

nsComboboxControlFrame::nsComboboxControlFrame()
  : nsAreaFrame() 
{
  mPresContext                 = nsnull;
  mFormFrame                   = nsnull;       
  mListControlFrame            = nsnull;
  mTextStr                     = "";
  mButtonContent               = nsnull;
  mDroppedDown                 = PR_FALSE;
  mDisplayFrame                = nsnull;
  mButtonFrame                 = nsnull;
  mDropdownFrame               = nsnull;
  mIgnoreFocus                 = PR_FALSE;
  mSelectedIndex               = -1;

  mCacheSize.width             = kSizeNotSet;
  mCacheSize.height            = kSizeNotSet;
  mCachedMaxElementSize.width  = kSizeNotSet;
  mCachedMaxElementSize.height = kSizeNotSet;
  mCachedAvailableSize.width   = kSizeNotSet;
  mCachedAvailableSize.height  = kSizeNotSet;
  mCachedUncDropdownSize.width  = kSizeNotSet;
  mCachedUncDropdownSize.height = kSizeNotSet;
  mCachedUncComboSize.width    = kSizeNotSet;
  mCachedUncComboSize.height   = kSizeNotSet;
  mItemDisplayWidth             = 0;
   //Shrink the area around it's contents
  //SetFlags(NS_BLOCK_SHRINK_WRAP);

  REFLOW_COUNTER_INIT()
}

//--------------------------------------------------------------
nsComboboxControlFrame::~nsComboboxControlFrame()
{
  REFLOW_COUNTER_DUMP("nsCCF");

  if (mFormFrame) {
    mFormFrame->RemoveFormControlFrame(*this);
    mFormFrame = nsnull;
  }
  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mButtonContent);

  nsFormControlFrame::RegUnRegAccessKey(mPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
}

//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsComboboxControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kIComboboxControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIComboboxControlFrame*) this);
    return NS_OK;
  } else if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  } else if (aIID.Equals(kIAnonymousContentCreatorIID)) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;                                        
    return NS_OK;   
  } else if (aIID.Equals(NS_GET_IID(nsISelectControlFrame))) {
    *aInstancePtr = (void *)(nsISelectControlFrame*)this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void *)(nsIStatefulFrame*)this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIRollupListener))) {
    *aInstancePtr = (void*)(nsIRollupListener*)this;
    return NS_OK;
  }
  return nsAreaFrame::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP
nsComboboxControlFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
   // Need to hold on the pres context because it is used later in methods
   // which don't have it passed in.
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);
  return nsAreaFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

//--------------------------------------------------------------
PRBool
nsComboboxControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}

// If nothing is selected, and we have options, select item 0
// This is a UI decision that goes against the HTML 4 spec.
// See bugzilla bug 15841 for justification of this deviation.
NS_IMETHODIMP
nsComboboxControlFrame::MakeSureSomethingIsSelected(nsIPresContext* aPresContext)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if (NS_SUCCEEDED(rv) && fcFrame) {
    // If nothing selected, and there are options, default selection to item 0
   rv = mListControlFrame->GetSelectedIndex(&mSelectedIndex);
    if (NS_SUCCEEDED(rv) && (mSelectedIndex < 0)) {
       // Find out if there are any options in the list to select
      PRInt32 length = 0;
      mListControlFrame->GetNumberOfOptions(&length);
      if (length > 0) {
         // Set listbox selection to first item in the list box
        rv = fcFrame->SetProperty(aPresContext, nsHTMLAtoms::selectedindex, "0");
        mSelectedIndex = 0;
      } else {
        UpdateSelection(PR_FALSE, PR_TRUE, mSelectedIndex); // Needed to reflow when removing last option
      }
    }
    
    // Don't NS_RELEASE fcFrame here as it isn't addRef'd in the QI (???)
  }
  return rv;
}  

// Initialize the text string in the combobox using either the current
// selection in the list box or the first item item in the list box.
void 
nsComboboxControlFrame::InitTextStr(nsIPresContext* aPresContext, PRBool aUpdate)
{
  MakeSureSomethingIsSelected(aPresContext);

  // Update the selected text string
  mListControlFrame->GetSelectedItem(mTextStr);

   // Update the display by setting the value attribute
  mDisplayContent->SetText(mTextStr.GetUnicode(), mTextStr.Length(), aUpdate);
}

//--------------------------------------------------------------

// Reset the combo box back to it original state.

void 
nsComboboxControlFrame::Reset(nsIPresContext* aPresContext)
{
  if (mPresState) {
    nsIStatefulFrame* sFrame = nsnull;
    nsresult res = mListControlFrame->QueryInterface(NS_GET_IID(nsIStatefulFrame),
                                                     (void**)&sFrame);
    if (NS_SUCCEEDED(res) && sFrame) {
      res = sFrame->RestoreState(mPresContext, mPresState);
      NS_RELEASE(sFrame);
    }
    mPresState = do_QueryInterface(nsnull);
  }

   // Reset the dropdown list to its original state
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_OK == result) && (nsnull != fcFrame)) {
    fcFrame->Reset(aPresContext); 
  }
 
    // Update the combobox using the text string returned from the dropdown list
  InitTextStr(aPresContext, PR_TRUE);
}

void 
nsComboboxControlFrame::InitializeControl(nsIPresContext* aPresContext)
{
  Reset(aPresContext);
}

//--------------------------------------------------------------
NS_IMETHODIMP 
nsComboboxControlFrame::GetType(PRInt32* aType) const
{
  *aType = NS_FORM_SELECT;
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult rv;
  rv = GetContent(&content);
  aContent = content;
  return rv;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetFont(nsIPresContext* aPresContext, 
                                const nsFont*&  aFont)
{
  return nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return 0;
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return 0;
}


//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                                 float aPixToTwip, 
                                                 nscoord aInnerHeight) const
{
   return 0;
}

//--------------------------------------------------------------
nscoord 
nsComboboxControlFrame::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}



void 
nsComboboxControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  //XXX: TODO Implement focus for combobox. 
}

void
nsComboboxControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));

    presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
  }
}


void
nsComboboxControlFrame::ShowPopup(PRBool aShowPopup)
{
  //XXX: This is temporary. It simulates a psuedo dropdown states by using a attribute selector 
  // This will not be need if the event state supports active states for use with the dropdown list
  // Currently, the event state manager will reset the active state to the content which has focus
  // which causes the active state to be removed from the event state manager immediately after it
  // it the select is set to the active state.
  
  nsCOMPtr<nsIAtom> activeAtom ( dont_QueryInterface(NS_NewAtom(kMozDropdownActive)));
  if (PR_TRUE == aShowPopup) {
    mContent->SetAttribute(kNameSpaceID_None, activeAtom, "", PR_TRUE);
  } else {
    mContent->UnsetAttribute(kNameSpaceID_None, activeAtom, PR_TRUE);
  }
}

// Show the dropdown list

void 
nsComboboxControlFrame::ShowList(nsIPresContext* aPresContext, PRBool aShowList)
{
  nsCOMPtr<nsIWidget> widget;

  // Get parent view
  nsIFrame * listFrame;
  if (NS_OK == mListControlFrame->QueryInterface(kIFrameIID, (void **)&listFrame)) {
    nsIView * view = nsnull;
    listFrame->GetView(aPresContext, &view);
    NS_ASSERTION(view != nsnull, "nsComboboxControlFrame view is null");
    if (view) {
    	view->GetWidget(*getter_AddRefs(widget));
    }
  }

  if (PR_TRUE == aShowList) {
    ShowPopup(PR_TRUE);
    mDroppedDown = PR_TRUE;

     // The listcontrol frame will call back to the nsComboboxControlFrame's ListWasSelected
     // which will stop the capture.
    mListControlFrame->AboutToDropDown();
    mListControlFrame->CaptureMouseEvents(aPresContext, PR_TRUE);

  } else {
    ShowPopup(PR_FALSE);
    mDroppedDown = PR_FALSE;
  }

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  presShell->FlushPendingNotifications();

  if (widget)
    widget->CaptureRollupEvents((nsIRollupListener *)this, mDroppedDown, PR_TRUE);

}


//-------------------------------------------------------------
// this is in response to the MouseClick from the containing browse button
// XXX: TODO still need to get filters from accept attribute
void 
nsComboboxControlFrame::MouseClicked(nsIPresContext* aPresContext)
{
   //ToggleList(aPresContext);
}


nsresult
nsComboboxControlFrame::ReflowComboChildFrame(nsIFrame* aFrame, 
                                             nsIPresContext*  aPresContext, 
                                             nsHTMLReflowMetrics&     aDesiredSize,
                                             const nsHTMLReflowState& aReflowState, 
                                             nsReflowStatus&          aStatus,
                                             nscoord                  aAvailableWidth,
                                             nscoord                  aAvailableHeight)
{
   // Constrain the child's width and height to aAvailableWidth and aAvailableHeight
  nsSize availSize(aAvailableWidth, aAvailableHeight);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, aFrame,
                                   availSize);
  kidReflowState.mComputedWidth = aAvailableWidth;
  kidReflowState.mComputedHeight = aAvailableHeight;
      
   // Reflow child
  nsRect rect;
  aFrame->GetRect(rect);
  nsresult rv = ReflowChild(aFrame, aPresContext, aDesiredSize, kidReflowState,
                            rect.x, rect.y, 0, aStatus);
 
   // Set the child's width and height to it's desired size
  FinishReflowChild(aFrame, aPresContext, aDesiredSize, rect.x, rect.y, 0);
  return rv;
}

// Suggest a size for the child frame. 
// Only frames which implement the nsIFormControlFrame interface and
// honor the SetSuggestedSize method will be placed and sized correctly.

void 
nsComboboxControlFrame::SetChildFrameSize(nsIFrame* aFrame, nscoord aWidth, nscoord aHeight) 
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = aFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if (NS_SUCCEEDED(result) && (nsnull != fcFrame)) {
    fcFrame->SetSuggestedSize(aWidth, aHeight); 
  }
}

nsresult 
nsComboboxControlFrame::GetPrimaryComboFrame(nsIPresContext* aPresContext, nsIContent* aContent, nsIFrame** aFrame)
{
  nsresult rv = NS_OK;
   // Get the primary frame from the presentation shell.
  nsCOMPtr<nsIPresShell> presShell;
  rv = aPresContext->GetShell(getter_AddRefs(presShell));
  if (NS_SUCCEEDED(rv) && presShell) {
    presShell->GetPrimaryFrameFor(aContent, aFrame);
  }
  return rv;
}

nsIFrame*
nsComboboxControlFrame::GetButtonFrame(nsIPresContext* aPresContext)
{
  if (mButtonFrame == nsnull) {
    NS_ASSERTION(mButtonContent != nsnull, "nsComboboxControlFrame mButtonContent is null");
    GetPrimaryComboFrame(aPresContext, mButtonContent, &mButtonFrame);
  }

  return mButtonFrame;
}

nsIFrame* 
nsComboboxControlFrame::GetDropdownFrame()
{
  return mDropdownFrame;
}


nsresult 
nsComboboxControlFrame::GetScreenHeight(nsIPresContext* aPresContext,
                                        nscoord& aHeight)
{
  aHeight = 0;
  nsIDeviceContext* context;
  aPresContext->GetDeviceContext( &context );
	if ( nsnull != context )
	{
		PRInt32 height;
    PRInt32 width;
		context->GetDeviceSurfaceDimensions(width, height);
		float devUnits;
 		context->GetDevUnitsToAppUnits(devUnits);
		aHeight = NSToIntRound(float( height) / devUnits );
		NS_RELEASE( context );
		return NS_OK;
	}

  return NS_ERROR_FAILURE;
}

int counter = 0;

nsresult 
nsComboboxControlFrame::PositionDropdown(nsIPresContext* aPresContext, 
                                         nscoord aHeight, 
                                         nsRect aAbsoluteTwipsRect, 
                                         nsRect aAbsolutePixelRect)
{
   // Position the dropdown list. It is positioned below the display frame if there is enough
   // room on the screen to display the entire list. Otherwise it is placed above the display
   // frame.

   // Note: As first glance, it appears that you could simply get the absolute bounding box for the
   // dropdown list by first getting it's view, then getting the view's nsIWidget, then asking the nsIWidget
   // for it's AbsoluteBounds. The problem with this approach, is that the dropdown lists y location can
   // change based on whether the dropdown is placed below or above the display frame.
   // The approach, taken here is to get use the absolute position of the display frame and use it's location
   // to determine if the dropdown will go offscreen.

   // Use the height calculated for the area frame so it includes both
   // the display and button heights.
  nsresult rv = NS_OK;
  nsIFrame* dropdownFrame = GetDropdownFrame();

  nscoord dropdownYOffset = aHeight;
// XXX: Enable this code to debug popping up above the display frame, rather than below it
  nsRect dropdownRect;
  dropdownFrame->GetRect(dropdownRect);

  nscoord screenHeightInPixels = 0;
  if (NS_SUCCEEDED(GetScreenHeight(aPresContext, screenHeightInPixels))) {
     // Get the height of the dropdown list in pixels.
     float t2p;
     aPresContext->GetTwipsToPixels(&t2p);
     nscoord absoluteDropDownHeight = NSTwipsToIntPixels(dropdownRect.height, t2p);
    
      // Check to see if the drop-down list will go offscreen
    if (NS_SUCCEEDED(rv) && ((aAbsolutePixelRect.y + aAbsolutePixelRect.height + absoluteDropDownHeight) > screenHeightInPixels)) {
      // move the dropdown list up
      dropdownYOffset = - (dropdownRect.height);
    }
  } 
 
  dropdownRect.x = 0;
  dropdownRect.y = dropdownYOffset; 
  nsRect currentRect;
  dropdownFrame->GetRect(currentRect);

  dropdownFrame->SetRect(aPresContext, dropdownRect);
#ifdef DEBUG_rodsXXXXXX
  printf("%d Position Dropdown at: %d %d %d %d\n", counter++, dropdownRect.x, dropdownRect.y, dropdownRect.width, dropdownRect.height);
#endif

  return rv;
}


// Calculate a frame's position in screen coordinates
nsresult
nsComboboxControlFrame::GetAbsoluteFramePosition(nsIPresContext* aPresContext,
                                                 nsIFrame *aFrame, 
                                                 nsRect& aAbsoluteTwipsRect, 
                                                 nsRect& aAbsolutePixelRect)
{
  //XXX: This code needs to take the view's offset into account when calculating
  //the absolute coordinate of the frame.
  nsresult rv = NS_OK;
 
  aFrame->GetRect(aAbsoluteTwipsRect);
  // zero these out, 
  // because the GetOffsetFromView figures them out
  aAbsoluteTwipsRect.x = 0;
  aAbsoluteTwipsRect.y = 0;

    // Get conversions between twips and pixels
  float t2p;
  float p2t;
  aPresContext->GetTwipsToPixels(&t2p);
  aPresContext->GetPixelsToTwips(&p2t);
  
   // Add in frame's offset from it it's containing view
  nsIView *containingView = nsnull;
  nsPoint offset;
  rv = aFrame->GetOffsetFromView(aPresContext, offset, &containingView);
  if (NS_SUCCEEDED(rv) && (nsnull != containingView)) {
    aAbsoluteTwipsRect.x += offset.x;
    aAbsoluteTwipsRect.y += offset.y;

    nsPoint viewOffset;
    containingView->GetPosition(&viewOffset.x, &viewOffset.y);
    nsIView * parent;
    containingView->GetParent(parent);
    while (nsnull != parent) {
      nsPoint po;
      parent->GetPosition(&po.x, &po.y);
      viewOffset.x += po.x;
      viewOffset.y += po.y;
      nsIScrollableView * scrollView;
      if (NS_OK == containingView->QueryInterface(NS_GET_IID(nsIScrollableView), (void **)&scrollView)) {
        nscoord x;
        nscoord y;
        scrollView->GetScrollPosition(x, y);
        viewOffset.x -= x;
        viewOffset.y -= y;
      }
      nsIWidget * widget;
      parent->GetWidget(widget);
      if (nsnull != widget) {
        // Add in the absolute offset of the widget.
        nsRect absBounds;
        nsRect lc;
        widget->WidgetToScreen(lc, absBounds);
        // Convert widget coordinates to twips   
        aAbsoluteTwipsRect.x += NSIntPixelsToTwips(absBounds.x, p2t);
        aAbsoluteTwipsRect.y += NSIntPixelsToTwips(absBounds.y, p2t);   
        NS_RELEASE(widget);
        break;
      }
      parent->GetParent(parent);
    }
    aAbsoluteTwipsRect.x += viewOffset.x;
    aAbsoluteTwipsRect.y += viewOffset.y;
  }

   // convert to pixel coordinates
  if (NS_SUCCEEDED(rv)) {
   aAbsolutePixelRect.x = NSTwipsToIntPixels(aAbsoluteTwipsRect.x, t2p);
   aAbsolutePixelRect.y = NSTwipsToIntPixels(aAbsoluteTwipsRect.y, t2p);
   aAbsolutePixelRect.width = NSTwipsToIntPixels(aAbsoluteTwipsRect.width, t2p);
   aAbsolutePixelRect.height = NSTwipsToIntPixels(aAbsoluteTwipsRect.height, t2p);
  }

  return rv;
}

////////////////////////////////////////////////////////////////
// Experimental Reflow
////////////////////////////////////////////////////////////////
#if defined(DO_NEW_REFLOW) || defined(DO_REFLOW_COUNTER)
//---------------------------------------------------------
// Returns the nsIDOMHTMLOptionElement for a given index 
// in the select's collection
//---------------------------------------------------------
static nsIDOMHTMLOptionElement* 
GetOption(nsIDOMHTMLCollection& aCollection, PRInt32 aIndex)
{
  nsIDOMNode* node = nsnull;
  if (NS_SUCCEEDED(aCollection.Item(aIndex, &node))) {
    if (nsnull != node) {
      nsIDOMHTMLOptionElement* option = nsnull;
      node->QueryInterface(NS_GET_IID(nsIDOMHTMLOptionElement), (void**)&option);
      NS_RELEASE(node);
      return option;
    }
  }
  return nsnull;
}
//---------------------------------------------------------
// for a given piece of content it returns nsIDOMHTMLSelectElement object
// or null 
//---------------------------------------------------------
static nsIDOMHTMLSelectElement* 
GetSelect(nsIContent * aContent)
{
  nsIDOMHTMLSelectElement* selectElement = nsnull;
  nsresult result = aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                             (void**)&selectElement);
  if (NS_SUCCEEDED(result) && selectElement) {
    return selectElement;
  } else {
    return nsnull;
  }
}
//---------------------------------------------------------
//---------------------------------------------------------
// This returns the collection for nsIDOMHTMLSelectElement or
// the nsIContent object is the select is null  (AddRefs)
//---------------------------------------------------------
static nsIDOMHTMLCollection* 
GetOptions(nsIContent * aContent, nsIDOMHTMLSelectElement* aSelect = nsnull)
{
  nsIDOMNSHTMLOptionCollection* optCol = nsnull;
  nsIDOMHTMLCollection* options = nsnull;
  if (!aSelect) {
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement = getter_AddRefs(GetSelect(aContent));
    if (selectElement) {
      selectElement->GetOptions(&optCol);  // AddRefs (1)
    }
  } else {
    aSelect->GetOptions(&optCol); // AddRefs (1)
  }
  if (optCol) {
    nsresult res = optCol->QueryInterface(NS_GET_IID(nsIDOMHTMLCollection), (void **)&options); // AddRefs (2)
    NS_RELEASE(optCol); // Release (1)
  }
  return options;
}

#ifdef DO_NEW_REFLOW
NS_IMETHODIMP 
nsComboboxControlFrame::ReflowItems(nsIPresContext* aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics& aDesiredSize) 
{
  //printf("*****************\n");
  const nsStyleFont* dspFont;
  mDisplayFrame->GetStyleData(eStyleStruct_Font,  (const nsStyleStruct *&)dspFont);
  nsCOMPtr<nsIDeviceContext> deviceContext;
  aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));
  NS_ASSERTION(deviceContext, "Couldn't get the device context"); 
  nsIFontMetrics * fontMet;
  deviceContext->GetMetricsFor(dspFont->mFont, fontMet);

  nscoord visibleHeight;
  //nsCOMPtr<nsIFontMetrics> fontMet;
  //nsresult res = nsFormControlHelper::GetFrameFontFM(aPresContext, this, getter_AddRefs(fontMet));
  if (fontMet) {
    fontMet->GetHeight(visibleHeight);
  }
 
  nsAutoString maxStr;
  nscoord maxWidth = 0;
  //nsIRenderingContext * rc = aReflowState.rendContext;
  nsresult rv = NS_ERROR_FAILURE; 
  nsCOMPtr<nsIDOMHTMLCollection> options = getter_AddRefs(GetOptions(mContent));
  if (options) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    //printf("--- Num of Items %d ---\n", numOptions);
    for (PRUint32 i=0;i<numOptions;i++) {
      nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = getter_AddRefs(GetOption(*options, i));
      if (optionElement) {
        nsAutoString text;
        rv = optionElement->GetLabel(text);
        if (NS_CONTENT_ATTR_HAS_VALUE != rv || 0 == text.Length()) {
          if (NS_OK == optionElement->GetText(text)) {
            nscoord width;
            aReflowState.rendContext->GetWidth(text, width);
            if (width > maxWidth) {
              maxStr = text;
              maxWidth = width;
            }
            //maxWidth = PR_MAX(width, maxWidth);
            //printf("[%d] - %d %s \n", i, width, text.ToNewCString());
          }
        }          
      }
    }
  }
  if (maxWidth == 0) {
    maxWidth = 11 * 15;
  }
  char * str = maxStr.ToNewCString();
  printf("id: %d maxWidth %d [%s]\n", mReflowId, maxWidth, str);
  delete [] str;

  // get the borderPadding for the display area
  const nsStyleSpacing* dspSpacing;
  mDisplayFrame->GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)dspSpacing);
  nsMargin dspBorderPadding;
  dspBorderPadding.SizeTo(0, 0, 0, 0);
  dspSpacing->CalcBorderPaddingFor(mDisplayFrame, dspBorderPadding);

  nscoord frmWidth  = maxWidth+dspBorderPadding.left+dspBorderPadding.right+
                      aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
  nscoord frmHeight = visibleHeight+dspBorderPadding.top+dspBorderPadding.bottom+
                      aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;

#if 1
  aDesiredSize.width  = frmWidth;
  aDesiredSize.height = frmHeight;
#else
  printf("Size %d,%d   %d,%d   %d,%d  %d,%d\n", 
         frmWidth, frmHeight, 
         aDesiredSize.width, aDesiredSize.height,
         frmWidth-aDesiredSize.width, frmHeight-aDesiredSize.height,
         (frmWidth-aDesiredSize.width)/15, (frmHeight-aDesiredSize.height)/15);
#endif
  NS_RELEASE(fontMet);
  return NS_OK;
}
#endif

#endif

//------------------------------------------------------------------
// This Method reflow just the contents of the ComboBox
// The contents are a Block frame containing a Text Frame - This is the display area
// and then the GfxButton - The dropdown button
//--------------------------------------------------------------------------
void 
nsComboboxControlFrame::ReflowCombobox(nsIPresContext *         aPresContext,
                                           const nsHTMLReflowState& aReflowState,
                                           nsHTMLReflowMetrics&     aDesiredSize,
                                           nsReflowStatus&          aStatus,
                                           nsIFrame *               aDisplayFrame,
                                           nsIFrame *               aDropDownBtn,
                                           nscoord&                 aDisplayWidth,
                                           nscoord                  aBtnWidth,
                                           const nsMargin&          aBorderPadding,
                                           nscoord                  aFallBackHgt,
                                           PRBool                   aCheckHeight)
{
  // start out by using the cached height
  // XXX later this will change when we better handle constrained height 
  nscoord dispHeight = mCacheSize.height - aBorderPadding.top - aBorderPadding.bottom;
  nscoord dispWidth  = aDisplayWidth;

  REFLOW_NOISY_MSG3("+++1 AdjustCombo DW:%d DH:%d  ", PX(dispWidth), PX(dispHeight));
  REFLOW_NOISY_MSG3("BW:%d  BH:%d  ", PX(aBtnWidth), PX(dispHeight));
  REFLOW_NOISY_MSG3("mCacheSize.height:%d - %d\n", PX(mCacheSize.height), PX((aBorderPadding.top + aBorderPadding.bottom)));

  // get the border and padding for the DisplayArea (block frame & textframe)
  const nsStyleSpacing* dspSpacing;
  aDisplayFrame->GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)dspSpacing);
  nsMargin dspBorderPadding;
  dspBorderPadding.SizeTo(0, 0, 0, 0);
  dspSpacing->CalcBorderPaddingFor(aDisplayFrame, dspBorderPadding);

  // adjust the height
  if (mCacheSize.height == kSizeNotSet) {
    if (aFallBackHgt == kSizeNotSet) {
      NS_ASSERTION(aFallBackHgt != kSizeNotSet, "Fallback can't be kSizeNotSet when mCacheSize.height == kSizeNotSet");
    } else {
      dispHeight = aFallBackHgt;
      REFLOW_NOISY_MSG2("+++3 Adding (dspBorderPadding.top + dspBorderPadding.bottom): %d\n", (dspBorderPadding.top + dspBorderPadding.bottom));
      dispHeight += (dspBorderPadding.top + dspBorderPadding.bottom);
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // XXX - This is really bad, but at the moment I am NOT adding in the left padding
  // we need to be able to find out what the left padding is on an option
  // and subtract that out before we can add in our padding.
  REFLOW_NOISY_MSG2("+++3 Adding: %d\n", dspBorderPadding.right);
  dispWidth += dspBorderPadding.left + dspBorderPadding.right;

  REFLOW_NOISY_MSG3("+++2 AdjustCombo DW:%d DH:%d  ", PX(dispWidth), PX(dispHeight));
  REFLOW_NOISY_MSG3(" BW:%d  BH:%d\n", PX(aBtnWidth), PX(dispHeight));

  // This sets the button to be a specific size
  // so no matter what it reflows at these values
  SetChildFrameSize(aDropDownBtn, aBtnWidth, dispHeight);

  // now that we know what the overall display width & height will be
  // set up a new reflow state and reflow the area frame at that size
  nsSize availSize(dispWidth + aBorderPadding.left + aBorderPadding.right, 
                   dispHeight + aBorderPadding.top + aBorderPadding.bottom);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, this, availSize);
  kidReflowState.mComputedWidth  = dispWidth;
  kidReflowState.mComputedHeight = dispHeight;

  // do reflow
  nsAreaFrame::Reflow(aPresContext, aDesiredSize, kidReflowState, aStatus);

  /////////////////////////////////////////////////////////
  // The DisplayFrame is a Block frame containing a TextFrame
  // and it is completely anonymous, so we must manually reflow it
  nsSize txtAvailSize(dispWidth - aBtnWidth, dispHeight);
  nsHTMLReflowMetrics txtKidSize(&txtAvailSize);
  nsHTMLReflowState   txtKidReflowState(aPresContext, aReflowState, aDisplayFrame, txtAvailSize);

  aDisplayFrame->WillReflow(aPresContext);
  aDisplayFrame->MoveTo(aPresContext, aBorderPadding.left, aBorderPadding.top);
  nsIView*  view;
  aDisplayFrame->GetView(aPresContext, &view);
  if (view) {
    nsAreaFrame::PositionFrameView(aPresContext, aDisplayFrame, view);
  }
  nsReflowStatus status;
  nsresult rv = aDisplayFrame->Reflow(aPresContext, txtKidSize, txtKidReflowState, status);

  /////////////////////////////////////////////////////////
  // If we are Constrained then the AreaFrame Reflow is the correct size
  // if we are unconstrained then 
  //if (aReflowState.mComputedWidth == NS_UNCONSTRAINEDSIZE) {
  //  aDesiredSize.width += txtKidSize.width;
  //}

  // Apparently, XUL lays out differently than HTML 
  // (the code above works for HTML and not XUL), 
  // so instead of using the above calculation
  // I just set it to what it should be.
  aDesiredSize.width = availSize.width;
  //aDesiredSize.height = availSize.height;

  // now we need to adjust layout, because the AreaFrame
  // doesn't position things exactly where we want them
  nscoord insideWidth  = dispWidth;
  nscoord insideHeight = aDesiredSize.height - aBorderPadding.top - aBorderPadding.bottom;

  // the gets for the Display "block" frame and for the button
  // make adjustments
  nsRect buttonRect;
  nsRect displayRect;
  aDisplayFrame->GetRect(displayRect);
  aDropDownBtn->GetRect(buttonRect);
  // set the display rect to be left justifed and 
  // fills the entire area except the button
  nscoord x          = aBorderPadding.left;
  displayRect.x      = x;
  displayRect.y      = aBorderPadding.top;
  displayRect.height = insideHeight;
  displayRect.width  = dispWidth - aBtnWidth;
  aDisplayFrame->SetRect(aPresContext, displayRect);
  x                 += displayRect.width;

  // right justify the button
  buttonRect.x       = x;
  buttonRect.y       = aBorderPadding.top;
  buttonRect.height  = insideHeight;
  buttonRect.width   = aBtnWidth;
  aDropDownBtn->SetRect(aPresContext, buttonRect);

  // since we have changed the height of the button 
  // make sure it has these new values
  // XXX this may not be needed anymore
  SetChildFrameSize(aDropDownBtn, aBtnWidth, aDesiredSize.height);


  REFLOW_NOISY_MSG3("**AdjustCombobox - Reflow: WW: %d  HH: %d\n", aDesiredSize.width, aDesiredSize.height);

  // Now cache the available height as our height without border and padding
  // This sets up the optimization for if a new available width comes in and we are equal or
  // less than it we can bail
  if (aDesiredSize.width != mCacheSize.width || aDesiredSize.height != mCacheSize.height) {
    if (aReflowState.availableWidth != NS_UNCONSTRAINEDSIZE) {
      mCachedAvailableSize.width  = aDesiredSize.width - (aBorderPadding.left + aBorderPadding.right);
    }
    if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
      mCachedAvailableSize.height = aDesiredSize.height - (aBorderPadding.top + aBorderPadding.bottom);
    }
    nsFormControlFrame::SetupCachedSizes(mCacheSize, mCachedMaxElementSize, aDesiredSize);
  }

  ///////////////////////////////////////////////////////////////////
  // This is an experimental reflow that is turned off in the build
#ifdef DO_NEW_REFLOW_X
  ReflowItems(aPresContext, aReflowState, aDesiredSize);
#endif
  ///////////////////////////////////////////////////////////////////
}

//----------------------------------------------------------
// 
//----------------------------------------------------------
#ifdef DO_REFLOW_DEBUG
static int myCounter = 0;

static void printSize(char * aDesc, nscoord aSize) 
{
  printf(" %s: ", aDesc);
  if (aSize == NS_UNCONSTRAINEDSIZE) {
    printf("UC");
  } else {
    printf("%d", PX(aSize));
  }
}
#endif

//-------------------------------------------------------------------
//-- Main Reflow for the Combobox
//-------------------------------------------------------------------
NS_IMETHODIMP 
nsComboboxControlFrame::Reflow(nsIPresContext*          aPresContext, 
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState, 
                               nsReflowStatus&          aStatus)
{
  aStatus = NS_FRAME_COMPLETE;

  REFLOW_COUNTER_REQUEST();

#ifdef DO_REFLOW_DEBUG
  printf("-------------Starting Combobox Reflow ----------------------------\n");
  printf("%p ** Id: %d nsCCF::Reflow %d R: ", this, mReflowId, myCounter++);
  switch (aReflowState.reason) {
    case eReflowReason_Initial:
      printf("Ini");break;
    case eReflowReason_Incremental:
      printf("Inc");break;
    case eReflowReason_Resize:
      printf("Rsz");break;
    case eReflowReason_StyleChange:
      printf("Sty");break;
    case eReflowReason_Dirty:
      printf("Drt ");
      break;
    default:printf("<unknown>%d", aReflowState.reason);break;
  }
  
  printSize("AW", aReflowState.availableWidth);
  printSize("AH", aReflowState.availableHeight);
  printSize("CW", aReflowState.mComputedWidth);
  printSize("CH", aReflowState.mComputedHeight);

  nsCOMPtr<nsIDOMHTMLCollection> optionsTemp = getter_AddRefs(GetOptions(mContent));
  PRUint32 numOptions;
  optionsTemp->GetLength(&numOptions);
  printSize("NO", (nscoord)numOptions);

  printf(" *\n");

#endif


  PRBool bailOnWidth;
  PRBool bailOnHeight;

  // Do initial check to see if we can bail out
  // If it is an Initial or Incremental Reflow we never bail out here
  // XXX right now we only bail if the width meets the criteria
  //
  // We bail:
  //   if mComputedWidth == NS_UNCONSTRAINEDSIZE and
  //      availableWidth == NS_UNCONSTRAINEDSIZE and 
  //      we have cached an available size
  //
  // We bail:
  //   if mComputedWidth == NS_UNCONSTRAINEDSIZE and
  //      availableWidth != NS_UNCONSTRAINEDSIZE and 
  //      availableWidth minus its border equals our cached available size
  //
  // We bail:
  //   if mComputedWidth != NS_UNCONSTRAINEDSIZE and
  //      cached availableSize.width == aReflowState.mComputedWidth and 
  //      cached AvailableSize.width == aCacheSize.width
  //
  // NOTE: this returns whether we are doing an Incremental reflow
  nsFormControlFrame::SkipResizeReflow(mCacheSize, 
                                       mCachedMaxElementSize, 
                                       mCachedAvailableSize, 
                                       aDesiredSize, aReflowState, 
                                       aStatus, 
                                       bailOnWidth, bailOnHeight);
  if (bailOnWidth) {
#ifdef DO_REFLOW_DEBUG // check or size
    const nsStyleSpacing* spacing;
    GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
    nsMargin borderPadding;
    borderPadding.SizeTo(0, 0, 0, 0);
    spacing->CalcBorderPaddingFor(this, borderPadding);
    UNCONSTRAINED_CHECK();
#endif
    REFLOW_DEBUG_MSG3("^** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
    return NS_OK;
  }

  // add ourself to the form control
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*,this));
    if (NS_FAILED(CreateDisplayFrame(aPresContext))) {
      return NS_ERROR_FAILURE;
    }
  }

  // Go get all of the important frame
  nsresult rv = NS_OK;
  nsIFrame* buttonFrame   = GetButtonFrame(aPresContext);
  nsIFrame* dropdownFrame = GetDropdownFrame();

  // Don't try to do any special sizing and positioning unless all of the frames
  // have been created.
  if ((nsnull == mDisplayFrame) ||
     (nsnull == buttonFrame) ||
     (nsnull == dropdownFrame)) 
  {
     // Since combobox frames are missing just do a normal area frame reflow
    return nsAreaFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }

  // size of each part of the combo box
  nsRect displayRect;
  nsRect buttonRect;
  nsRect dropdownRect;

  // get our border and padding, 
  // XXX - should be the same mComputedBorderPadding?
  // maybe we should use that?
  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
  nsMargin borderPadding;
  borderPadding.SizeTo(0, 0, 0, 0);
  spacing->CalcBorderPaddingFor(this, borderPadding);

   // Get the current sizes of the combo box child frames
  mDisplayFrame->GetRect(displayRect);
  buttonFrame->GetRect(buttonRect);
  dropdownFrame->GetRect(dropdownRect);

  // We should cache this instead getting it everytime
  // the default size of the of scrollbar
  // that will be the default width of the dropdown button
  // the height will be the height of the text
  nscoord scrollbarWidth = -1;
  nsCOMPtr<nsIDeviceContext> dx;
  aPresContext->GetDeviceContext(getter_AddRefs(dx));
  if (dx) { 
    // Get the width in Device pixels (in this case screen)
    SystemAttrStruct info;
    dx->GetSystemAttribute(eSystemAttr_Size_ScrollbarWidth, &info);
    // Get the pixels to twips conversion for the current device (screen or printer)
    float p2t;
    aPresContext->GetPixelsToTwips(&p2t);
    // Get the scale factor for mapping from one device (screen) 
    //   to another device (screen or printer)
    // Typically when it is a screen the scale 1.0
    //   when it is a printer is could be anything
    float scale;
    dx->GetCanonicalPixelScale(scale); 
    scrollbarWidth = NSIntPixelsToTwips(info.mSize, p2t*scale);
  }  
  
  // set up a new reflow state for use throughout
  nsHTMLReflowState firstPassState(aReflowState);
  nsHTMLReflowMetrics dropdownDesiredSize(nsnull);

  // Check to see if this a fully unconstrained reflow
  PRBool fullyUnconstrained = firstPassState.availableWidth == NS_UNCONSTRAINEDSIZE &&
                              firstPassState.mComputedWidth == NS_UNCONSTRAINEDSIZE && 
                              firstPassState.availableHeight == NS_UNCONSTRAINEDSIZE &&
                              firstPassState.mComputedHeight == NS_UNCONSTRAINEDSIZE;

  PRBool forceReflow = PR_FALSE;

  // Only reflow the display and button 
  // if they are the target of the incremental reflow, unless they change size. 
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIFrame* targetFrame;
    firstPassState.reflowCommand->GetTarget(targetFrame);
    // Check to see if we are the target of the Incremental Reflow
    if (targetFrame == this) {
      // We need to check here to see if we can get away with just reflowing
      // the combobox and not the dropdown
      REFLOW_DEBUG_MSG("-----------------Target is Combobox------------\n");

      // If the mComputedWidth matches our cached display width 
      // then we get away with bailing out
      PRBool doFullReflow = firstPassState.mComputedWidth != NS_UNCONSTRAINEDSIZE &&
                            firstPassState.mComputedWidth != mItemDisplayWidth;
      if (!doFullReflow) {
        // OK, so we got lucky and the size didn't change
        // so do a simple reflow and bail out
        REFLOW_DEBUG_MSG("------------Reflowing AreaFrame and bailing----\n\n");
        ReflowCombobox(aPresContext, firstPassState, aDesiredSize, aStatus, 
                           mDisplayFrame, buttonFrame, mItemDisplayWidth, 
                           scrollbarWidth, borderPadding);
        REFLOW_COUNTER();
        UNCONSTRAINED_CHECK();
        REFLOW_DEBUG_MSG3("&** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
        return rv;
      }
      // Nope, something changed that affected our size 
      // so we need to do a full reflow and resize ourself
      REFLOW_DEBUG_MSG("------------Do Full Reflow----\n\n");
      firstPassState.reason = eReflowReason_StyleChange;
      firstPassState.reflowCommand = nsnull;
      forceReflow = PR_TRUE;

    } else {
      // Now, see if our target is the dropdown
      // If so, maybe an items was added or some style changed etc.
      //               OR
      // We get an Incremental reflow on the dropdown when it is being 
      // shown or hidden.
      if (targetFrame == dropdownFrame) {
        REFLOW_DEBUG_MSG("---------Target is Dropdown (Clearing Unc DD Size)---\n");
        PRBool bail = PR_FALSE;

        // Let's start by checking to see if this is a reflow ro showing or hiding.
        nsIView * view;
        dropdownFrame->GetView(aPresContext, &view);
        nsViewVisibility vis = nsViewVisibility_kHide;
        NS_ASSERTION(view != nsnull, "The dropdown frame doesn't have a view!");
        if (view != nsnull) {
          view->GetVisibility(vis);
        }
        // If the view is hidden, but the combobox thinks the it is dropped down
        // then it look like we are suppose to be showing the dropdown
        // XXX - It is really to bad we have to reflow at all 
        // just to get it to show up or be hidden
        if (nsViewVisibility_kHide == vis && mDroppedDown) {
          bail = PR_TRUE;
          // Do a constrained reflow at a predetermined size
          // to have the dropdown shown
          firstPassState.reason = eReflowReason_StyleChange;
          firstPassState.reflowCommand = nsnull;
          mListControlFrame->SetOverrideReflowOptimization(PR_TRUE);
          nscoord width = mCacheSize.width > kSizeNotSet?PR_MAX(mCacheSize.width, dropdownRect.width):NS_UNCONSTRAINEDSIZE;
          ReflowComboChildFrame(dropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, aStatus, width, NS_UNCONSTRAINEDSIZE);  

        } else if (nsViewVisibility_kShow == vis && !mDroppedDown) {
          bail = PR_TRUE;
          // Do a Unconstrained reflow to "roll" it up
          // then cache that unconstrained size
          firstPassState.reason = eReflowReason_StyleChange;
          firstPassState.reflowCommand = nsnull;
          mListControlFrame->SetOverrideReflowOptimization(PR_TRUE);
          ReflowComboChildFrame(dropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, aStatus, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);  
          // cache the values here also, 
          // since we just did an unconstrained reflow
          mCachedUncDropdownSize.width  = dropdownDesiredSize.width;
          mCachedUncDropdownSize.height = dropdownDesiredSize.height;
          REFLOW_DEBUG_MSG3("---2 Caching mCachedUncDropdownSize %d,%d\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 
        }

        // Ok, we get to bail because it was either being shown or hidden
        if (bail) {
          if (fullyUnconstrained) {
            aDesiredSize.width  = mCachedUncComboSize.width;
            aDesiredSize.height = mCachedUncComboSize.height;
          } else {
            aDesiredSize.width  = mCacheSize.width;
            aDesiredSize.height = mCacheSize.height;
          }

          if (aDesiredSize.maxElementSize != nsnull) {
            aDesiredSize.maxElementSize->width  = mCachedMaxElementSize.width;
            aDesiredSize.maxElementSize->height = mCachedMaxElementSize.height;
          }
          aDesiredSize.ascent = aDesiredSize.height;
          aDesiredSize.descent = 0;
          REFLOW_DEBUG_MSG3("|** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
          REFLOW_DEBUG_MSG("---- Setting Cached values and then bailing out\n\n");
          UNCONSTRAINED_CHECK();
          return NS_OK;
        }
        // Nope, we were unlucky so now we do a full reflow
        mCachedUncDropdownSize.width  = kSizeNotSet;
        mCachedUncDropdownSize.height = kSizeNotSet;       
        REFLOW_DEBUG_MSG("---- Doing Full Reflow\n");
        // This is an incremental reflow targeted at the dropdown list
        // and it didn't have anything to do with being show or hidden.
        // 
        // The incremental reflow will not get to the dropdown list 
        // because it is in the "popup" list 
        // when this flow of control drops out of this if it will do a reflow
        // on the AreaFrame which SHOULD make it get tothe drop down 
        // except that it is in the popup list, so we have it reflowed as
        // a StyleChange, this is not as effecient as doing an Incremental
        //
        // At this point we want to by pass the reflow optimization in the dropdown
        // because we aren't why it is getting an incremental reflow, but we do
        // know that it needs to be resized or restyled
        //mListControlFrame->SetOverrideReflowOptimization(PR_TRUE);

      } else if (targetFrame == mDisplayFrame || targetFrame == buttonFrame) {
        /*if (targetFrame == mDisplayFrame) {
          nsresult rv = ReflowComboChildFrame(mDisplayFrame, aPresContext, aDesiredSize, 
                                              aReflowState, aStatus,
                                              aReflowState.availableWidth, 
                                              aReflowState.availableHeight);
          return rv;
        }*/

        // The incremental reflow is targeted at either the block or the button
        REFLOW_DEBUG_MSG2("-----------------Target is %s------------\n", (targetFrame == mDisplayFrame?"DisplayItem Frame":"DropDown Btn Frame"));
        REFLOW_DEBUG_MSG("---- Doing AreaFrame Reflow and then bailing out\n");
        // Do simple reflow and bail out
        ReflowCombobox(aPresContext, firstPassState, aDesiredSize, aStatus, 
                           mDisplayFrame, buttonFrame, 
                           mItemDisplayWidth, scrollbarWidth, borderPadding, kSizeNotSet, PR_TRUE);
        REFLOW_DEBUG_MSG3("+** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
        REFLOW_COUNTER();
        UNCONSTRAINED_CHECK();
        return rv;
      } else {
        // Here the target of the reflow was a child of the dropdown list
        // so we must do a full reflow
        REFLOW_DEBUG_MSG("-----------------Target is Dropdown's Child (Option Item)------------\n");
        REFLOW_DEBUG_MSG("---- Doing Reflow as StyleChange\n");
      }
      firstPassState.reason = eReflowReason_StyleChange;
      firstPassState.reflowCommand = nsnull;
      mListControlFrame->SetOverrideReflowOptimization(PR_TRUE);
      forceReflow = PR_TRUE;
    }
  }

  // This ifdef is for the new approach to reflow 
  // where we don't reflow the dropdown
  // we just figure out or width from the list of items
  //
  // This next section is the Current implementation
  // the else contains the new reflow code
#ifndef DO_NEW_REFLOW 

  // Here is another special optimization
  // Only reflow the dropdown if it has never been reflowed unconstrained
  //
  // Or someone up above here may want to force it to be reflowed 
  // by setting one or both of these to kSizeNotSet
  if ((mCachedUncDropdownSize.width == kSizeNotSet && 
       mCachedUncDropdownSize.height == kSizeNotSet) || forceReflow) {
    REFLOW_DEBUG_MSG3("---Re %d,%d\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 
    // A width has not been specified for the select so size the display area
    // to match the width of the longest item in the drop-down list. The dropdown
    // list has already been reflowed and sized to shrink around its contents above.
    ReflowComboChildFrame(dropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, 
                          aStatus, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); 
    if (forceReflow) {
      mCachedUncDropdownSize.width  = dropdownDesiredSize.width;
      mCachedUncDropdownSize.height = dropdownDesiredSize.height;
    }
  } else {
    // Here we pretended we did an unconstrained reflow
    // so we set the cached values and continue on
    REFLOW_DEBUG_MSG3("--- Using Cached ListBox Size %d,%d\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 
    dropdownDesiredSize.width  = mCachedUncDropdownSize.width;
    dropdownDesiredSize.height = mCachedUncDropdownSize.height;
  }

  /////////////////////////////////////////////////////////////////////////
  // XXX - I need to clean this nect part up a little it is very redundant

  // Check here to if this is a mComputed unconstrained reflow
  PRBool computedUnconstrained = firstPassState.mComputedWidth == NS_UNCONSTRAINEDSIZE && 
                                 firstPassState.mComputedHeight == NS_UNCONSTRAINEDSIZE;
  if (computedUnconstrained && !forceReflow) {
    // Because Incremental reflows aren't actually getting to the dropdown
    // we cache the size from when it did a fully unconstrained reflow
    // we then check to see if the size changed at all, 
    // if not then bail out we don't need to worry
    if (mCachedUncDropdownSize.width == kSizeNotSet && mCachedUncDropdownSize.height == kSizeNotSet) {
      mCachedUncDropdownSize.width  = dropdownDesiredSize.width;
      mCachedUncDropdownSize.height = dropdownDesiredSize.height;
      REFLOW_DEBUG_MSG3("---1 Caching mCachedUncDropdownSize %d,%d\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 

    } else if (mCachedUncDropdownSize.width == dropdownDesiredSize.width &&
               mCachedUncDropdownSize.height == dropdownDesiredSize.height) {

      if (mCachedUncComboSize.width != kSizeNotSet && mCachedUncComboSize.height != kSizeNotSet) {
        REFLOW_DEBUG_MSG3("--- Bailing because of mCachedUncDropdownSize %d,%d\n\n", PX(mCachedUncDropdownSize.width), PX(mCachedUncDropdownSize.height)); 
        aDesiredSize.width  = mCachedUncComboSize.width;
        aDesiredSize.height = mCachedUncComboSize.height;

        if (aDesiredSize.maxElementSize != nsnull) {
          aDesiredSize.maxElementSize->width  = mCachedMaxElementSize.width;
          aDesiredSize.maxElementSize->height = mCachedMaxElementSize.height;
        }
        aDesiredSize.ascent = aDesiredSize.height;
        aDesiredSize.descent = 0;
        UNCONSTRAINED_CHECK();
        REFLOW_DEBUG_MSG3("#** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
        return NS_OK;
      }
    } else {
      mCachedUncDropdownSize.width  = dropdownDesiredSize.width;
      mCachedUncDropdownSize.height = dropdownDesiredSize.height;
    }
  }
  // clean up stops here
  /////////////////////////////////////////////////////////////////////////

  // So this point we know we flowed the dropdown unconstrained
  // now we get to figure out how big we need to be and 
  // 
  // We don't reflow the combobox here at the new size
  // we cache its new size and reflow it on the dropdown
  nsSize size;
  PRInt32 length = 0;
  mListControlFrame->GetNumberOfOptions(&length);

  // dropdownRect will hold the content size (minus border padding) 
  // for the display area
  dropdownFrame->GetRect(dropdownRect);

  // Get maximum size of the largest item in the dropdown
  // The height of the display frame will be that height
  // the width will be the same as 
  // the dropdown width (minus its borderPadding) OR
  // a caculation off the mComputedWidth from reflow
  mListControlFrame->GetMaximumSize(size);

  // the variable "size" will now be 
  // the default size of the dropdown btn
  if (scrollbarWidth > 0) {
    size.width = scrollbarWidth;
  }

  // Get the border and padding for the dropdown
  const nsStyleSpacing* dropSpacing;
  dropdownFrame->GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)dropSpacing);
  nsMargin dropBorderPadding;
  dropBorderPadding.SizeTo(0, 0, 0, 0);
  dropSpacing->CalcBorderPaddingFor(dropdownFrame, dropBorderPadding);

  // get the borderPadding for the display area
  const nsStyleSpacing* dspSpacing;
  mDisplayFrame->GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)dspSpacing);
  nsMargin dspBorderPadding;
  dspBorderPadding.SizeTo(0, 0, 0, 0);
  dspSpacing->CalcBorderPaddingFor(mDisplayFrame, dspBorderPadding);

  // Substract dropdown's borderPadding from the width of the dropdown rect
  // to get the size of the content area
  //
  // the height will come from the mDisplayFrame's height
  // declare a size for the item display frame

  //Set the desired size for the button and display frame
  if (NS_UNCONSTRAINEDSIZE == firstPassState.mComputedWidth) {
    REFLOW_DEBUG_MSG("Unconstrained.....\n");
    REFLOW_DEBUG_MSG4("*B mItemDisplayWidth %d  dropdownRect.width:%d dropdownRect.w+h %d\n", PX(mItemDisplayWidth), PX(dropdownRect.width), PX((dropBorderPadding.left + dropBorderPadding.right)));

    // Start with the dropdown rect's width
    mItemDisplayWidth = dropdownRect.width;

    REFLOW_DEBUG_MSG2("*  mItemDisplayWidth %d\n", PX(mItemDisplayWidth));

    // subtract off the display's BorderPadding
    mItemDisplayWidth -= dspBorderPadding.left + dspBorderPadding.right;

    REFLOW_DEBUG_MSG2("*A mItemDisplayWidth %d\n", PX(mItemDisplayWidth));

  } else {
    REFLOW_DEBUG_MSG("Constrained.....\n");
    if (firstPassState.mComputedWidth > 0) {
      // Compute the display item's width from reflow's mComputedWidth
      // mComputedWidth has already excluded border and padding
      // so subtract off the button's size
      REFLOW_DEBUG_MSG3("B mItemDisplayWidth %d    %d\n", PX(mItemDisplayWidth), PX(dspBorderPadding.right));
      mItemDisplayWidth = firstPassState.mComputedWidth - dspBorderPadding.left - dspBorderPadding.right;
      REFLOW_DEBUG_MSG2("A mItemDisplayWidth %d\n", PX(mItemDisplayWidth));
      REFLOW_DEBUG_MSG4("firstPassState.mComputedWidth %d -  size.width %d dspBorderPadding.right %d\n", PX(firstPassState.mComputedWidth), PX(size.width), PX(dspBorderPadding.right));
    }
  }
  // this reflows and makes and last minute adjustments
  ReflowCombobox(aPresContext, firstPassState, aDesiredSize, aStatus, 
                     mDisplayFrame, buttonFrame, mItemDisplayWidth, scrollbarWidth, 
                     borderPadding, size.height);

  // The dropdown was reflowed UNCONSTRAINED before, now we need to check to see
  // if it needs to be resized. 
  //
  // Optimization - The style (font, etc.) maybe different for the display item 
  // than for any particular item in the dropdown. So, if the new size of combobox
  // is smaller than the dropdown, that is OK, The dropdown MUST always be either the same
  //size as the combo or larger if necessary
#if 0
  if (aDesiredSize.width > dropdownDesiredSize.width) {
    if (eReflowReason_Initial == firstPassState.reason) {
      firstPassState.reason = eReflowReason_Resize;
    }
    REFLOW_DEBUG_MSG3("*** Reflowing ListBox to width: %d it was %d\n", PX(aDesiredSize.width), PX(dropdownDesiredSize.width));
    // Reflow the dropdown list to match the width of the display + button
    ReflowComboChildFrame(dropdownFrame, aPresContext, dropdownDesiredSize, firstPassState, aStatus, 
                          aDesiredSize.width, NS_UNCONSTRAINEDSIZE);
  }
#endif

#else  // DO_NEW_REFLOW

  if (mCacheSize.width == kSizeNotSet) {
    ReflowItems(aPresContext, aReflowState, aDesiredSize);
  } else {
    aDesiredSize.width  = mCacheSize.width;
    aDesiredSize.height = mCacheSize.height;
  }

  // get the borderPadding for the display area
  const nsStyleSpacing* dspSpacing;
  mDisplayFrame->GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)dspSpacing);
  nsMargin dspBorderPadding;
  dspBorderPadding.SizeTo(0, 0, 0, 0);

  dspSpacing->CalcBorderPaddingFor(mDisplayFrame, dspBorderPadding);

  if (NS_UNCONSTRAINEDSIZE == firstPassState.mComputedWidth) {
    mItemDisplayWidth = aDesiredSize.width - (dspBorderPadding.left + dspBorderPadding.right);
    mItemDisplayWidth -= borderPadding.left + borderPadding.right;
  } else {
    if (firstPassState.mComputedWidth > 0) {
      // Compute the display item's width from reflow's mComputedWidth
      // mComputedWidth has already excluded border and padding
      // so subtract off the button's size
      mItemDisplayWidth = firstPassState.mComputedWidth - dspBorderPadding.left - dspBorderPadding.right;
    }
  }

  // this reflows and makes and last minute adjustments
  ReflowCombobox(aPresContext, firstPassState, aDesiredSize, aStatus, 
                     mDisplayFrame, buttonFrame, mItemDisplayWidth, scrollbarWidth, 
                     borderPadding, 
                     aDesiredSize.height- borderPadding.top - borderPadding.bottom -
                     dspBorderPadding.top - dspBorderPadding.bottom);
#endif // DO_NEW_REFLOW

  // Set the max element size to be the same as the desired element size.
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

#if 0
  COMPARE_QUIRK_SIZE("nsComboboxControlFrame", 127, 22) 
#endif

  // cache the availabe size to be our desired size minus the borders
  // this is so if our cached avilable size is ever equal to or less 
  // than the real avilable size we can bail out
  if (aReflowState.availableWidth != NS_UNCONSTRAINEDSIZE) {
    mCachedAvailableSize.width  = aDesiredSize.width - (borderPadding.left + borderPadding.right);
  }
  if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
    mCachedAvailableSize.height = aDesiredSize.height - (borderPadding.top + borderPadding.bottom);
  }

  nsFormControlFrame::SetupCachedSizes(mCacheSize, mCachedMaxElementSize, aDesiredSize);

  REFLOW_DEBUG_MSG3("** Done nsCCF DW: %d  DH: %d\n\n", PX(aDesiredSize.width), PX(aDesiredSize.height));
  REFLOW_COUNTER();
  UNCONSTRAINED_CHECK();

  // If this was a fully unconstrained reflow we cache 
  // the combobox's unconstrained size
  if (fullyUnconstrained) {
    mCachedUncComboSize.width = aDesiredSize.width;
    mCachedUncComboSize.height = aDesiredSize.height;
  }

  return rv;

}

//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
// Done with New Reflow
//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------

//--------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetName(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      nsHTMLValue value;
      result = formControl->GetHTMLAttribute(nsHTMLAtoms::name, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}

PRInt32 
nsComboboxControlFrame::GetMaxNumValues()
{
  return 1;
}
  
/*XXX-REMOVE
PRBool
nsComboboxControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                   nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  // use our name and the text widgets value 
  aNames[0] = name;
  aValues[0] = mTextStr;
  aNumValues = 1;
  nsresult status = PR_TRUE;
  return status;
}
*/

PRBool
nsComboboxControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                     nsString* aValues, nsString* aNames)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_SUCCEEDED(result)) && (nsnull != fcFrame)) {
    return fcFrame->GetNamesValues(aMaxNumValues, aNumValues, aValues, aNames);
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                         const nsPoint& aPoint,
                                         nsIFrame** aFrame)
{
  if (nsFormFrame::GetDisabled(this)) {
    *aFrame = this;
    return NS_OK;
  } else {
    return nsHTMLContainerFrame::GetFrameForPoint(aPresContext, aPoint, aFrame);
  }
  return NS_OK;
}


//--------------------------------------------------------------

#ifdef NS_DEBUG
NS_IMETHODIMP
nsComboboxControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ComboboxControl", aResult);
}
#endif


//----------------------------------------------------------------------
// nsIComboboxControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::ShowDropDown(PRBool aDoDropDown) 
{ 
  if (nsFormFrame::GetDisabled(this)) {
    return NS_OK;
  }

  if (!mDroppedDown && aDoDropDown) {
    // XXX Temporary for Bug 19416
    nsIFrame* dropdownFrame = GetDropdownFrame();
    nsIView * lstView;
    dropdownFrame->GetView(mPresContext, &lstView);
    if (lstView) {
      lstView->IgnoreSetPosition(PR_FALSE);
    }
    if (mListControlFrame) {
      mListControlFrame->SyncViewWithFrame(mPresContext);
    }
    // XXX Temporary for Bug 19416
    if (lstView) {
      lstView->IgnoreSetPosition(PR_TRUE);
    }
    ToggleList(mPresContext);
    return NS_OK;
  } else if (mDroppedDown && !aDoDropDown) {
    ToggleList(mPresContext);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsComboboxControlFrame::SetDropDown(nsIFrame* aDropDownFrame)
{
  mDropdownFrame = aDropDownFrame;
 
  if (NS_OK != mDropdownFrame->QueryInterface(kIListControlFrameIID, (void**)&mListControlFrame)) {

    return NS_ERROR_FAILURE;
  }

  // The ListControlFrame was just created and added to the comboxbox
  // so provide it with a PresState so it can restore itself
  // when it does its first "Reset"
  if (mPresState) {
    mListControlFrame->SetPresState(mPresState);
    mPresState = do_QueryInterface(nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetDropDown(nsIFrame** aDropDownFrame) 
{
  if (nsnull == aDropDownFrame) {
    return NS_ERROR_FAILURE;
  }

  *aDropDownFrame = mDropdownFrame;
 
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::ListWasSelected(nsIPresContext* aPresContext)
{
  if (aPresContext == nsnull) {
    aPresContext = mPresContext;
  }
  ShowList(aPresContext, PR_FALSE);
  mListControlFrame->CaptureMouseEvents(aPresContext, PR_FALSE);

  PRInt32 indx;
  mListControlFrame->GetSelectedIndex(&indx);

  UpdateSelection(PR_TRUE, PR_FALSE, indx);

  return NS_OK;
}
// Toggle dropdown list.

NS_IMETHODIMP 
nsComboboxControlFrame::ToggleList(nsIPresContext* aPresContext)
{

  ShowList(aPresContext, (PR_FALSE == mDroppedDown));

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::UpdateSelection(PRBool aDoDispatchEvent, PRBool aForceUpdate, PRInt32 aNewIndex)
{
  if (mListControlFrame) {
     // Check to see if the selection changed
    if (mSelectedIndex != aNewIndex || aForceUpdate) {
    //???if (mSelectedIndex != aNewIndex || (aForceUpdate && aNewIndex != kSizeNotSet)) {
      mListControlFrame->GetSelectedItem(mTextStr); // Update text box
#ifdef DO_REFLOW_DEBUG
      char * str =  mTextStr.ToNewCString();
     REFLOW_DEBUG_MSG2("UpdateSelection %s\n", str);
     delete [] str;
#endif
      mListControlFrame->UpdateSelection(aDoDispatchEvent, aForceUpdate, mContent);
    }
    mSelectedIndex = aNewIndex;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::AbsolutelyPositionDropDown()
{
  nsRect absoluteTwips;
  nsRect absolutePixels;

  nsRect rect;
  this->GetRect(rect);
  GetAbsoluteFramePosition(mPresContext, this,  absoluteTwips, absolutePixels);
  PositionDropdown(mPresContext, rect.height, absoluteTwips, absolutePixels);
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetAbsoluteRect(nsRect* aRect)
{
  nsRect absoluteTwips;
  nsRect rect;
  this->GetRect(rect);
  GetAbsoluteFramePosition(mPresContext, this,  absoluteTwips, *aRect);

  return NS_OK;
}

///////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsComboboxControlFrame::SelectionChanged()
{
  // Send reflow command because the new text maybe larger
  nsresult rv = NS_OK;
  if (mDisplayContent) {
    nsAutoString value;
    const nsTextFragment* fragment;
    nsresult result = mDisplayContent->GetText(&fragment);
    if (NS_SUCCEEDED(result)) {
      fragment->AppendTo(value);
    }
    PRBool shouldSetValue = PR_FALSE;
    if (NS_FAILED(result) || value.Length() == 0) {
      shouldSetValue = PR_TRUE;
    } else {
      shouldSetValue = value != mTextStr;
      REFLOW_DEBUG_MSG3("**** SelectionChanged  Old[%s]  New[%s]\n", value.ToNewCString(), mTextStr.ToNewCString());
    }
    if (shouldSetValue) {
      rv = mDisplayContent->SetText(mTextStr.GetUnicode(), mTextStr.Length(), PR_TRUE);
      nsFrameState state;
      mTextFrame->GetFrameState(&state);
      state |= NS_FRAME_IS_DIRTY;
      mTextFrame->SetFrameState(state);
      mDisplayFrame->GetFrameState(&state);
      state |= NS_FRAME_IS_DIRTY;
      mDisplayFrame->SetFrameState(state);
      nsCOMPtr<nsIPresShell> shell;
      rv = mPresContext->GetShell(getter_AddRefs(shell));
      ReflowDirtyChild(shell, (nsIFrame*) mDisplayFrame);
    }
  }
  return rv;
}


//----------------------------------------------------------------------
// nsISelectControlFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::DoneAddingContent(PRBool aIsDone)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = NS_ERROR_FAILURE;
  if (dropdownFrame != nsnull) {
    rv = dropdownFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame), 
                                                (void**)&listFrame);
    if (NS_SUCCEEDED(rv) && listFrame) {
      rv = listFrame->DoneAddingContent(aIsDone);
      NS_RELEASE(listFrame);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsComboboxControlFrame::AddOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
#ifdef DO_REFLOW_DEBUG
  printf("**********\n*********AddOption: %d\n", aIndex);
#endif
  nsISelectControlFrame* listFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = dropdownFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame), 
                                              (void**)&listFrame);
  if (NS_SUCCEEDED(rv) && listFrame) {
    rv = listFrame->AddOption(aPresContext, aIndex);
    //PRInt32 index;
    //mListControlFrame->GetSelectedIndex(&index);
    //UpdateSelection(PR_FALSE, PR_TRUE, index);
    NS_RELEASE(listFrame);
  }
  // If we added the first option, we might need to select it.
  // We should call MakeSureSomethingIsSelected here, but since it
  // it changes selection, which currently causes a reframe, and thus
  // deletes the frame out from under the caller, causing a crash. (Bug 17995)
  // XXX MakeSureSomethingIsSelected(aPresContext);
  return rv;
}
  

NS_IMETHODIMP
nsComboboxControlFrame::RemoveOption(nsIPresContext* aPresContext, PRInt32 aIndex)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = dropdownFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame), 
                                              (void**)&listFrame);
  if (NS_SUCCEEDED(rv) && listFrame) {
    rv = listFrame->RemoveOption(aPresContext, aIndex);
    NS_RELEASE(listFrame);
  }
  // If we removed the selected option, nothing is selected any more.
  // Restore selection to option 0 if there are options left.
  MakeSureSomethingIsSelected(aPresContext);
  return rv;
}

NS_IMETHODIMP
nsComboboxControlFrame::SetOptionSelected(PRInt32 aIndex, PRBool aValue)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = dropdownFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame), 
                                              (void**)&listFrame);
  if (NS_SUCCEEDED(rv) && listFrame) {
    rv = listFrame->SetOptionSelected(aIndex, aValue);
    NS_RELEASE(listFrame);
  }
  return rv;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetOptionSelected(PRInt32 aIndex, PRBool* aValue)
{
  nsISelectControlFrame* listFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult rv = dropdownFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame), 
                                              (void**)&listFrame);
  if (NS_SUCCEEDED(rv) && listFrame) {
    rv = listFrame->GetOptionSelected(aIndex, aValue);
    NS_RELEASE(listFrame);
  }
  return rv;
}

NS_IMETHODIMP 
nsComboboxControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }
  if (nsFormFrame::GetDisabled(this)) { 
    return NS_OK;
  }

  return NS_OK;
}


nsresult 
nsComboboxControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP 
nsComboboxControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_SUCCEEDED(result)) && (nsnull != fcFrame)) {
    return fcFrame->SetProperty(aPresContext, aName, aValue);
  }
  return result;
}

NS_IMETHODIMP 
nsComboboxControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsIFrame* dropdownFrame = GetDropdownFrame();
  nsresult result = dropdownFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_SUCCEEDED(result)) && (nsnull != fcFrame)) {
    return fcFrame->GetProperty(aName, aValue);
  }
  return result;
}


NS_IMETHODIMP 
nsComboboxControlFrame::CreateDisplayFrame(nsIPresContext* aPresContext)
{

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  nsresult rv = NS_NewBlockFrame(shell, (nsIFrame**)&mDisplayFrame, NS_BLOCK_SPACE_MGR);
  if (NS_FAILED(rv)) { return rv; }
  if (!mDisplayFrame) { return NS_ERROR_NULL_POINTER; }

  // create the style context for the anonymous frame
  nsCOMPtr<nsIStyleContext> styleContext;
  rv = aPresContext->ResolvePseudoStyleContextFor(mContent, 
                                                  nsHTMLAtoms::mozDisplayComboboxControlFrame,
                                                  mStyleContext,
                                                  PR_FALSE,
                                                  getter_AddRefs(styleContext));
  if (NS_FAILED(rv)) { return rv; }
  if (!styleContext) { return NS_ERROR_NULL_POINTER; }

  // create a text frame and put it inside the block frame
  rv = NS_NewTextFrame(shell, &mTextFrame);
  if (NS_FAILED(rv)) { return rv; }
  if (!mTextFrame) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIStyleContext> textStyleContext;
  rv = aPresContext->ResolvePseudoStyleContextFor(mContent, 
                                                  nsHTMLAtoms::mozDisplayComboboxControlFrame,
                                                  styleContext,
                                                  PR_FALSE,
                                                  getter_AddRefs(textStyleContext));
  if (NS_FAILED(rv)) { return rv; }
  if (!textStyleContext) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDisplayContent));
  mTextFrame->Init(aPresContext, content, mDisplayFrame, textStyleContext, nsnull);
  mTextFrame->SetInitialChildList(aPresContext, nsnull, nsnull);
  nsCOMPtr<nsIPresShell> presShell;
  rv = aPresContext->GetShell(getter_AddRefs(presShell));
  if (NS_FAILED(rv)) { return rv; }
  if (!presShell) { return NS_ERROR_NULL_POINTER; }
  nsCOMPtr<nsIFrameManager> frameManager;
  rv = presShell->GetFrameManager(getter_AddRefs(frameManager));
  if (NS_FAILED(rv)) { return rv; }
  if (!frameManager) { return NS_ERROR_NULL_POINTER; }
  frameManager->SetPrimaryFrameFor(content, mTextFrame);

  rv = mDisplayFrame->Init(aPresContext, content, this, styleContext, nsnull);
  if (NS_FAILED(rv)) { return rv; }

  mDisplayFrame->SetInitialChildList(aPresContext, nsnull, mTextFrame);

  return NS_OK;
}


NS_IMETHODIMP
nsComboboxControlFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                               nsISupportsArray& aChildList)
{
  // The frames used to display the combo box and the button used to popup the dropdown list
  // are created through anonymous content. The dropdown list is not created through anonymous
  // content because it's frame is initialized specifically for the drop-down case and it is placed
  // a special list referenced through NS_COMBO_FRAME_POPUP_LIST_INDEX to keep separate from the
  // layout of the display and button. 
  //
  // Note: The value attribute of the display content is set when an item is selected in the dropdown list.
  // If the content specified below does not honor the value attribute than nothing will be displayed.
  // In addition, if the frame created by content below for does not implement the nsIFormControlFrame 
  // interface and honor the SetSuggestedSize method the placement and size of the display area will not
  // match what is normally desired for a combobox.


  // For now the content that is created corresponds to two input buttons. It would be better to create the
  // tag as something other than input, but then there isn't any way to create a button frame since it
  // isn't possible to set the display type in CSS2 to create a button frame.

    // create content used for display
  //nsIAtom* tag = NS_NewAtom("mozcombodisplay");

  // Add a child text content node for the label
  nsCOMPtr<nsIContent> labelContent;
  nsresult result = NS_NewTextNode(getter_AddRefs(labelContent));
  nsAutoString value="X";
  if (NS_SUCCEEDED(result) && labelContent) {
    // set the value of the text node
    mDisplayContent = do_QueryInterface(labelContent);
    mDisplayContent->SetText(value.GetUnicode(), value.Length(), PR_TRUE);

    nsIDocument* doc;
    mContent->GetDocument(doc);
    labelContent->SetDocument(doc, PR_FALSE);
    NS_RELEASE(doc);
    mContent->AppendChildTo(labelContent, PR_FALSE);

    // create button which drops the list down
    result = NS_NewHTMLInputElement(&mButtonContent, nsHTMLAtoms::input);
    //NS_ADDREF(mButtonContent);
    if (NS_SUCCEEDED(result) && mButtonContent) {
      mButtonContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, nsAutoString("button"), PR_FALSE);
      aChildList.AppendElement(mButtonContent);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsComboboxControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  return NS_OK;
}



NS_IMETHODIMP
nsComboboxControlFrame::Destroy(nsIPresContext* aPresContext)
{
   // Cleanup frames in popup child list
  mPopupFrames.DestroyFrames(aPresContext);
  if (mDisplayFrame) {
    mFrameConstructor->RemoveMappingsForFrameSubtree(aPresContext, mDisplayFrame, nsnull);
    mDisplayFrame->Destroy(aPresContext);
    mDisplayFrame=nsnull;
  }

  if (mDisplayContent) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDisplayContent));
    if (content) {
      PRInt32 index;
      if (NS_SUCCEEDED(mContent->IndexOf(content, index))) {
        mContent->RemoveChildAt(index, PR_FALSE);
      }
    }
  }

  return nsAreaFrame::Destroy(aPresContext);
}


NS_IMETHODIMP
nsComboboxControlFrame::FirstChild(nsIPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame**      aFirstChild) const
{
  if (nsLayoutAtoms::popupList == aListName) {
    *aFirstChild = mPopupFrames.FirstChild();
  } else {
    nsAreaFrame::FirstChild(aPresContext, aListName, aFirstChild);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;
  if (nsLayoutAtoms::popupList == aListName) {
    mPopupFrames.SetFrames(aChildList);
  } else {
    rv = nsAreaFrame::SetInitialChildList(aPresContext, aListName, aChildList);
    InitTextStr(aPresContext, PR_FALSE);
  }
  return rv;
}

NS_IMETHODIMP
nsComboboxControlFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom** aListName) const
{
   // Maintain a seperate child list for the dropdown list (i.e. popup listbox)
   // This is necessary because we don't want the listbox to be included in the layout
   // of the combox's children because it would take up space, when it is suppose to
   // be floating above the display.
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  if (aIndex <= NS_AREA_FRAME_ABSOLUTE_LIST_INDEX) {
    return nsAreaFrame::GetAdditionalChildListName(aIndex, aListName);
  }
  
  *aListName = nsnull;
  if (NS_COMBO_FRAME_POPUP_LIST_INDEX == aIndex) {
    *aListName = nsLayoutAtoms::popupList;
    NS_ADDREF(*aListName);
  }
  return NS_OK;
}

PRIntn
nsComboboxControlFrame::GetSkipSides() const
{    
    // Don't skip any sides during border rendering
  return 0;
}


//----------------------------------------------------------------------
  //nsIRollupListener
//----------------------------------------------------------------------
NS_IMETHODIMP 
nsComboboxControlFrame::Rollup()
{
  if (mDroppedDown) {
    mListControlFrame->AboutToRollup();
    ShowDropDown(PR_FALSE);
    mListControlFrame->CaptureMouseEvents(mPresContext, PR_FALSE);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIStatefulFrame
// XXX Do we need to implement this here?  It is already implemented in
//     the ListControlFrame, our child...
//----------------------------------------------------------------------
NS_IMETHODIMP
nsComboboxControlFrame::GetStateType(nsIPresContext* aPresContext,
                                     nsIStatefulFrame::StateType* aStateType)
{
  *aStateType = nsIStatefulFrame::eSelectType;
  return NS_OK;
}

NS_IMETHODIMP
nsComboboxControlFrame::SaveState(nsIPresContext* aPresContext, nsIPresState** aState)
{
  if (!mListControlFrame) {
    return NS_ERROR_UNEXPECTED;
  }

  // The ListControlFrame ignores requests to Save its state 
  // when it is owned by a combobox
  // so we call the internal SaveState here
  return mListControlFrame->SaveStateInternal(aPresContext, aState);

}

NS_IMETHODIMP
nsComboboxControlFrame::RestoreState(nsIPresContext* aPresContext, nsIPresState* aState)
{
  // The ListControlFrame ignores requests to Restore its state 
  // when it is owned by a combobox
  // so we cache it here if the frame hasn't been created yet
  // or we call the internal RestoreState here
  if (!mListControlFrame) {
    mPresState = aState;
    return NS_OK;
  }

  return mListControlFrame->RestoreStateInternal(aPresContext, aState);
}

NS_METHOD 
nsComboboxControlFrame::Paint(nsIPresContext* aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect,
                             nsFramePaintLayer aWhichLayer)
{
#ifdef NOISY
  printf("%p paint layer %d at (%d, %d, %d, %d)\n", this, aWhichLayer, 
    aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
#endif
  nsAreaFrame::Paint(aPresContext,aRenderingContext,aDirtyRect,aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    nsRect rect(0, 0, mRect.width, mRect.height);
    if (mDisplayFrame) {
      aRenderingContext.PushState();
      PRBool clipEmpty;
      nsRect clipRect;
      mDisplayFrame->GetRect(clipRect);
      aRenderingContext.SetClipRect(clipRect, nsClipCombine_kReplace, clipEmpty);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, 
                 mDisplayFrame, NS_FRAME_PAINT_LAYER_BACKGROUND);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, 
                 mDisplayFrame, NS_FRAME_PAINT_LAYER_FOREGROUND);
      aRenderingContext.PopState(clipEmpty);
    }
  }
  //nsRect rect(0, 0, 50000, 50000);
	//aRenderingContext.SetColor(NS_RGB(255, 0, 0));
	//aRenderingContext.FillRect(rect);
  
  return NS_OK;
}


