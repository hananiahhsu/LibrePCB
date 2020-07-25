/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * https://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "boardclipboarddata.h"

#include <librepcb/project/circuit/netsignal.h>

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

BoardClipboardData::BoardClipboardData(const Uuid&  boardUuid,
                                       const Point& cursorPos) noexcept
  : mBoardUuid(boardUuid),
    mCursorPos(cursorPos),
    mNetSegments(),
    mPlanes(),
    mPolygons(),
    mStrokeTexts(),
    mHoles() {
}

BoardClipboardData::BoardClipboardData(const QByteArray& mimeData)
  : BoardClipboardData(Uuid::createRandom(), Point()) {
  SExpression root = SExpression::parse(mimeData, FilePath());
  mBoardUuid       = root.getValueByPath<Uuid>("board");
  mCursorPos       = Point(root.getChildByPath("cursor_position"));
  mNetSegments.loadFromSExpression(root);
  mPlanes.loadFromSExpression(root);
  mPolygons.loadFromSExpression(root);
  mStrokeTexts.loadFromSExpression(root);
  mHoles.loadFromSExpression(root);
}

BoardClipboardData::~BoardClipboardData() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

std::unique_ptr<QMimeData> BoardClipboardData::toMimeData() const {
  SExpression sexpr =
      serializeToDomElement("librepcb_clipboard_board");  // can throw

  std::unique_ptr<QMimeData> data(new QMimeData());
  data->setData(getMimeType(), sexpr.toByteArray());
  data->setText(sexpr.toByteArray());
  return data;
}

std::unique_ptr<BoardClipboardData> BoardClipboardData::fromMimeData(
    const QMimeData* mime) {
  QByteArray content = mime ? mime->data(getMimeType()) : QByteArray();
  if (!content.isNull()) {
    return std::unique_ptr<BoardClipboardData>(
        new BoardClipboardData(content));  // can throw
  } else {
    return nullptr;
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void BoardClipboardData::serialize(SExpression& root) const {
  root.appendChild(mCursorPos.serializeToDomElement("cursor_position"), true);
  root.appendChild("board", mBoardUuid, true);
  mNetSegments.serialize(root);
  mPlanes.serialize(root);
  mPolygons.serialize(root);
  mStrokeTexts.serialize(root);
  mHoles.serialize(root);
}

QString BoardClipboardData::getMimeType() noexcept {
  return QString("application/x-librepcb-clipboard.board; version=%1")
      .arg(qApp->applicationVersion());
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb
