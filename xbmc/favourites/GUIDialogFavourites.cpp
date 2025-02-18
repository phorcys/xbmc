/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogFavourites.h"

#include "ContextMenuManager.h"
#include "ServiceBroker.h"
#include "Util.h"
#include "dialogs/GUIDialogContextMenu.h"
#include "favourites/GUIWindowFavourites.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "input/actions/ActionIDs.h"

#define FAVOURITES_LIST 450

CGUIDialogFavourites::CGUIDialogFavourites() :
    CGUIDialog(WINDOW_DIALOG_FAVOURITES, "DialogFavourites.xml"),
    m_favouritesService(CServiceBroker::GetFavouritesService())
{
  m_favourites = new CFileItemList;
  m_loadType = KEEP_IN_MEMORY;
}

CGUIDialogFavourites::~CGUIDialogFavourites(void)
{
  delete m_favourites;
}

bool CGUIDialogFavourites::OnMessage(CGUIMessage &message)
{
  if (message.GetMessage() == GUI_MSG_CLICKED)
  {
    if (message.GetSenderId() == FAVOURITES_LIST)
    {
      int item = GetSelectedItem();
      int action = message.GetParam1();
      if (action == ACTION_SELECT_ITEM || action == ACTION_MOUSE_LEFT_CLICK)
        OnClick(item);
      else if (action == ACTION_MOVE_ITEM_UP)
        OnMoveItem(item, -1);
      else if (action == ACTION_MOVE_ITEM_DOWN)
        OnMoveItem(item, 1);
      else if (action == ACTION_CONTEXT_MENU || action == ACTION_MOUSE_RIGHT_CLICK)
        OnPopupMenu(item);
      else if (action == ACTION_DELETE_ITEM)
        OnDelete(item);
      else
        return false;
      return true;
    }
  }
  else if (message.GetMessage() == GUI_MSG_WINDOW_DEINIT)
  {
    CGUIDialog::OnMessage(message);
    // clear our favourites
    CGUIMessage message(GUI_MSG_LABEL_RESET, GetID(), FAVOURITES_LIST);
    OnMessage(message);
    m_favourites->Clear();
    return true;
  }
  return CGUIDialog::OnMessage(message);
}

void CGUIDialogFavourites::OnInitWindow()
{
  m_favouritesService.GetAll(*m_favourites);
  UpdateList();
  CGUIWindow::OnInitWindow();
}

int CGUIDialogFavourites::GetSelectedItem()
{
  CGUIMessage message(GUI_MSG_ITEM_SELECTED, GetID(), FAVOURITES_LIST);
  OnMessage(message);
  return message.GetParam1();
}

void CGUIDialogFavourites::OnClick(int item)
{
  if (item < 0 || item >= m_favourites->Size())
    return;

  CGUIMessage message(GUI_MSG_EXECUTE, 0, GetID());
  message.SetStringParam(CUtil::GetExecPath(*(*m_favourites)[item], std::to_string(GetID())));

  Close();

  CServiceBroker::GetGUI()->GetWindowManager().SendMessage(message);
}

void CGUIDialogFavourites::OnPopupMenu(int item)
{
  if (item < 0 || item >= m_favourites->Size())
    return;

  // highlight the item
  (*m_favourites)[item]->Select(true);

  CContextButtons choices;
  if (m_favourites->Size() > 1)
  {
    choices.Add(1, 13332); // Move up
    choices.Add(2, 13333); // Move down
  }
  choices.Add(3, 20019); // Choose thumbnail
  choices.Add(4, 118); // Rename
  choices.Add(5, 15015); // Remove

  CFileItemPtr itemPtr = m_favourites->Get(item);

  //temporary workaround until the context menu ids are removed
  const int addonItemOffset = 10000;

  auto addonItems = CServiceBroker::GetContextMenuManager().GetAddonItems(*itemPtr);

  for (size_t i = 0; i < addonItems.size(); ++i)
    choices.Add(addonItemOffset + i, addonItems[i]->GetLabel(*itemPtr));

  int button = CGUIDialogContextMenu::ShowAndGetChoice(choices);

  // unhighlight the item
  (*m_favourites)[item]->Select(false);

  if (button == 1)
    OnMoveItem(item, -1);
  else if (button == 2)
    OnMoveItem(item, +1);
  else if (button == 3)
    OnSetThumb(item);
  else if (button == 4)
    OnRename(item);
  else if (button == 5)
    OnDelete(item);
  else if (button >= addonItemOffset)
    CONTEXTMENU::LoopFrom(*addonItems.at(button - addonItemOffset), itemPtr);
}

void CGUIDialogFavourites::OnMoveItem(int item, int amount)
{
  if (item < 0 || item >= m_favourites->Size() || m_favourites->Size() <= 1 || 0 == amount) return;

  int nextItem = (item + amount) % m_favourites->Size();
  if (nextItem < 0) nextItem += m_favourites->Size();

  m_favourites->Swap(item, nextItem);
  m_favouritesService.Save(*m_favourites);

  CGUIMessage message(GUI_MSG_ITEM_SELECT, GetID(), FAVOURITES_LIST, nextItem);
  OnMessage(message);

  UpdateList();
}

void CGUIDialogFavourites::OnDelete(int item)
{
  if (item < 0 || item >= m_favourites->Size())
    return;
  m_favourites->Remove(item);
  m_favouritesService.Save(*m_favourites);

  CGUIMessage message(GUI_MSG_ITEM_SELECT, GetID(), FAVOURITES_LIST, item < m_favourites->Size() ? item : item - 1);
  OnMessage(message);

  UpdateList();
}

void CGUIDialogFavourites::OnRename(int item)
{
  if (item < 0 || item >= m_favourites->Size())
    return;

  if (CGUIWindowFavourites::ChooseAndSetNewName(*(*m_favourites)[item]))
  {
    m_favouritesService.Save(*m_favourites);
    UpdateList();
  }
}

void CGUIDialogFavourites::OnSetThumb(int item)
{
  if (item < 0 || item >= m_favourites->Size())
    return;

  if (CGUIWindowFavourites::ChooseAndSetNewThumbnail(*(*m_favourites)[item]))
  {
    m_favouritesService.Save(*m_favourites);
    UpdateList();
  }
}

void CGUIDialogFavourites::UpdateList()
{
  int currentItem = GetSelectedItem();
  CGUIMessage message(GUI_MSG_LABEL_BIND, GetID(), FAVOURITES_LIST, currentItem >= 0 ? currentItem : 0, 0, m_favourites);
  OnMessage(message);
}

CFileItemPtr CGUIDialogFavourites::GetCurrentListItem(int offset)
{
  int currentItem = GetSelectedItem();
  if (currentItem < 0 || !m_favourites->Size()) return CFileItemPtr();

  int item = (currentItem + offset) % m_favourites->Size();
  if (item < 0) item += m_favourites->Size();
  return (*m_favourites)[item];
}
