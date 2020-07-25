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

#ifndef LIBREPCB_PROJECT_EDITOR_BOARDCLIPBOARDDATA_H
#define LIBREPCB_PROJECT_EDITOR_BOARDCLIPBOARDDATA_H

/*******************************************************************************
 *  Includes
 ******************************************************************************/

#include <librepcb/common/fileio/serializableobject.h>
#include <librepcb/common/fileio/serializableobjectlist.h>
#include <librepcb/common/geometry/hole.h>
#include <librepcb/common/geometry/polygon.h>
#include <librepcb/common/geometry/stroketext.h>
#include <librepcb/common/signalslot.h>
#include <librepcb/common/units/all_length_units.h>
#include <librepcb/common/uuid.h>
#include <librepcb/project/boards/items/bi_plane.h>
#include <librepcb/project/boards/items/bi_via.h>
#include <librepcb/project/circuit/circuit.h>

#include <QtCore>
#include <QtWidgets>

#include <memory>

/*******************************************************************************
 *  Namespace / Forward Declarations
 ******************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*******************************************************************************
 *  Class BoardClipboardData
 ******************************************************************************/

/**
 * @brief The BoardClipboardData class
 */
class BoardClipboardData final : public SerializableObject {
public:
  // Types
  struct Via : public SerializableObject {
    static constexpr const char* tagname = "via";

    Uuid           uuid;
    Point          position;
    BI_Via::Shape  shape;
    PositiveLength size;
    PositiveLength drillDiameter;
    Signal<Via>    onEdited;  ///< Dummy event, not used

    Via(const Uuid& uuid, const Point& position, BI_Via::Shape shape,
        const PositiveLength& size, const PositiveLength& drillDiameter)
      : uuid(uuid),
        position(position),
        shape(shape),
        size(size),
        drillDiameter(drillDiameter),
        onEdited(*this) {}

    explicit Via(const SExpression& node)
      : uuid(node.getChildByIndex(0).getValue<Uuid>()),
        position(node.getChildByPath("position")),
        shape(node.getValueByPath<BI_Via::Shape>("shape")),
        size(node.getValueByPath<PositiveLength>("size")),
        drillDiameter(node.getValueByPath<PositiveLength>("drill")),
        onEdited(*this) {}

    /// @copydoc ::librepcb::SerializableObject::serialize()
    void serialize(SExpression& root) const override {
      root.appendChild(uuid);
      root.appendChild(position.serializeToDomElement("position"), true);
      root.appendChild("size", size, false);
      root.appendChild("drill", drillDiameter, false);
      root.appendChild("shape", shape, false);
    }
  };

  struct NetPoint : public SerializableObject {
    static constexpr const char* tagname = "junction";

    Uuid             uuid;
    Point            position;
    Signal<NetPoint> onEdited;  ///< Dummy event, not used

    NetPoint(const Uuid& uuid, const Point& position)
      : uuid(uuid), position(position), onEdited(*this) {}

    explicit NetPoint(const SExpression& node)
      : uuid(node.getChildByIndex(0).getValue<Uuid>()),
        position(node.getChildByPath("position")),
        onEdited(*this) {}

    /// @copydoc ::librepcb::SerializableObject::serialize()
    void serialize(SExpression& root) const override {
      root.appendChild(uuid);
      root.appendChild(position.serializeToDomElement("position"), true);
    }
  };

  struct NetLine : public SerializableObject {
    static constexpr const char* tagname = "trace";

    Uuid               uuid;
    tl::optional<Uuid> startJunction;
    tl::optional<Uuid> startVia;
    tl::optional<Uuid> endJunction;
    tl::optional<Uuid> endVia;
    QString            layer;
    PositiveLength     width;
    Signal<NetLine>    onEdited;  ///< Dummy event, not used

    explicit NetLine(const Uuid& uuid, const QString& layer,
                     const PositiveLength& width)
      : uuid(uuid),
        startJunction(),
        startVia(),
        endJunction(),
        endVia(),
        layer(layer),
        width(width),
        onEdited(*this) {}

    explicit NetLine(const SExpression& node)
      : uuid(node.getChildByIndex(0).getValue<Uuid>()),
        layer(node.getValueByPath<QString>("layer", true)),
        width(node.getValueByPath<PositiveLength>("width")),
        onEdited(*this) {
      if (node.tryGetChildByPath("from/via")) {
        startVia = node.getValueByPath<Uuid>("from/via");
      } else {
        startJunction = node.getValueByPath<Uuid>("from/junction");
      }
      if (node.tryGetChildByPath("to/via")) {
        endVia = node.getValueByPath<Uuid>("to/via");
      } else {
        endJunction = node.getValueByPath<Uuid>("to/junction");
      }
    }

    /// @copydoc ::librepcb::SerializableObject::serialize()
    void serialize(SExpression& root) const override {
      root.appendChild(uuid);
      root.appendChild("layer", SExpression::createToken(layer), false);
      root.appendChild("width", width, false);
      SExpression& from = root.appendList("from", true);
      if (startVia) {
        from.appendChild("via", *startVia, false);
      } else {
        from.appendChild("junction", *startJunction, false);
      }
      SExpression& to = root.appendList("to", true);
      if (endVia) {
        to.appendChild("via", *endVia, false);
      } else {
        to.appendChild("junction", *endJunction, false);
      }
    }
  };

  struct NetSegment : public SerializableObject {
    static constexpr const char* tagname = "netsegment";

    CircuitIdentifier                          netName;
    SerializableObjectList<Via, Via>           vias;
    SerializableObjectList<NetPoint, NetPoint> points;
    SerializableObjectList<NetLine, NetLine>   lines;
    Signal<NetSegment> onEdited;  ///< Dummy event, not used

    explicit NetSegment(const CircuitIdentifier& netName)
      : netName(netName), vias(), points(), lines(), onEdited(*this) {}

    explicit NetSegment(const SExpression& node)
      : netName(node.getValueByPath<CircuitIdentifier>("net")),
        vias(node),
        points(node),
        lines(node),
        onEdited(*this) {}

    /// @copydoc ::librepcb::SerializableObject::serialize()
    void serialize(SExpression& root) const override {
      root.appendChild("net", netName, true);
      vias.serialize(root);
      points.serialize(root);
      lines.serialize(root);
    }
  };

  struct Plane : public SerializableObject {
    static constexpr const char* tagname = "plane";

    Uuid                   uuid;
    QString                layer;
    QString                netSignalName;
    Path                   outline;
    UnsignedLength         minWidth;
    UnsignedLength         minClearance;
    bool                   keepOrphans;
    int                    priority;
    BI_Plane::ConnectStyle connectStyle;
    Signal<Plane>          onEdited;  ///< Dummy event, not used

    Plane(const Uuid& uuid, const QString& layer, const QString& netSignalName,
          const Path& outline, const UnsignedLength& minWidth,
          const UnsignedLength& minClearance, bool keepOrphans, int priority,
          BI_Plane::ConnectStyle connectStyle)
      : uuid(uuid),
        layer(layer),
        netSignalName(netSignalName),
        outline(outline),
        minWidth(minWidth),
        minClearance(minClearance),
        keepOrphans(keepOrphans),
        priority(priority),
        connectStyle(connectStyle),
        onEdited(*this) {}

    explicit Plane(const SExpression& node)
      : uuid(node.getChildByIndex(0).getValue<Uuid>()),
        layer(node.getValueByPath<QString>("layer", true)),
        netSignalName(node.getValueByPath<QString>("net", true)),
        outline(node),
        minWidth(node.getValueByPath<UnsignedLength>("min_width")),
        minClearance(node.getValueByPath<UnsignedLength>("min_clearance")),
        keepOrphans(node.getValueByPath<bool>("keep_orphans")),
        priority(node.getValueByPath<int>("priority")),
        connectStyle(
            node.getValueByPath<BI_Plane::ConnectStyle>("connect_style")),
        onEdited(*this) {}

    /// @copydoc ::librepcb::SerializableObject::serialize()
    void serialize(SExpression& root) const override {
      root.appendChild(uuid);
      root.appendChild("layer", layer, false);
      root.appendChild("net", netSignalName, true);
      root.appendChild("priority", priority, false);
      root.appendChild("min_width", minWidth, true);
      root.appendChild("min_clearance", minClearance, false);
      root.appendChild("keep_orphans", keepOrphans, false);
      root.appendChild("connect_style", connectStyle, true);
      outline.serialize(root);
    }
  };

  // Constructors / Destructor
  BoardClipboardData()                                = delete;
  BoardClipboardData(const BoardClipboardData& other) = delete;
  BoardClipboardData(const Uuid& boardUuid, const Point& cursorPos) noexcept;
  explicit BoardClipboardData(const QByteArray& mimeData);
  ~BoardClipboardData() noexcept;

  // Getters
  const Uuid&  getBoardUuid() const noexcept { return mBoardUuid; }
  const Point& getCursorPos() const noexcept { return mCursorPos; }
  SerializableObjectList<NetSegment, NetSegment>& getNetSegments() noexcept {
    return mNetSegments;
  }
  SerializableObjectList<Plane, Plane>& getPlanes() noexcept { return mPlanes; }
  PolygonList&    getPolygons() noexcept { return mPolygons; }
  StrokeTextList& getStrokeTexts() noexcept { return mStrokeTexts; }
  HoleList&       getHoles() noexcept { return mHoles; }

  // General Methods
  std::unique_ptr<QMimeData>                 toMimeData() const;
  static std::unique_ptr<BoardClipboardData> fromMimeData(
      const QMimeData* mime);

  // Operator Overloadings
  BoardClipboardData& operator=(const BoardClipboardData& rhs) = delete;

private:  // Methods
  /// @copydoc ::librepcb::SerializableObject::serialize()
  void serialize(SExpression& root) const override;

  static QString getMimeType() noexcept;

private:  // Data
  Uuid                                           mBoardUuid;
  Point                                          mCursorPos;
  SerializableObjectList<NetSegment, NetSegment> mNetSegments;
  SerializableObjectList<Plane, Plane>           mPlanes;
  PolygonList                                    mPolygons;
  StrokeTextList                                 mStrokeTexts;
  HoleList                                       mHoles;
};

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace project
}  // namespace librepcb

#endif
