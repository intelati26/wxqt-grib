/*
#
#  Copyright 2013-present Roman Kushnarenko
#  Licensed under the Apache License, Version 2.0 (the 'License')
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  http://www.apache.org/licenses/LICENSE-2.0
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an 'AS IS' BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#  https://github.com/sromku/polygon-contains-point
#
#
#   The 2D polygon. <br>
#   @see {@link Builder}
#   @author Roman Kushnarenko (sromku@gmail.com)
#*/

#include "ExternalPolygon.h"
#include "external/Builder.h"

ExternalPolygon::ExternalPolygon(const vector<ExternalLine>& sides, BoundingBox boundingBox)
    : sides{sides}
    , boundingBox{boundingBox}
{}

bool ExternalPolygon::inBoundingBox(ExternalPoint point) const {
    if (point.x < boundingBox.xMin || point.x > boundingBox.xMax || point.y < boundingBox.yMin || point.y > boundingBox.yMax)
        return false;
    else
        return true;
}

bool ExternalPolygon::contains(ExternalPoint point) const {
    if (inBoundingBox(point)) {
        const auto ray = createRay(point);
        auto intersection = 0;
        for (const auto& side : sides) {
            if (intersect(ray, side)) {
                intersection += 1;
            }
        }
        // If the number of intersections is odd, then the point is inside the polygon
        if (intersection % 2 == 1) {
            return true;
        }
    }
    return false;
}

ExternalLine ExternalPolygon::createRay(ExternalPoint point) const {
    // create outside point
    const auto epsilon = (boundingBox.xMax - boundingBox.xMin) / 100.0f;
    const auto outsidePoint = ExternalPoint{boundingBox.xMin - epsilon, boundingBox.yMin};
    const auto vector = ExternalLine{outsidePoint, point};
    return vector;
}

bool ExternalPolygon::intersect(const ExternalLine& ray, const ExternalLine& side) {
    ExternalPoint intersectPoint;
    // if both vectors aren't from the kind of x=1 lines then go into
    if (!ray.isVertical() && !side.isVertical()) {
        // check if both vectors are parallel. If they are parallel then no intersection point will exist
        if ((ray.getA() - side.getA()) == 0) {
            return false;
        }
        const auto x = ((side.getB() - ray.getB()) / (ray.getA() - side.getA()));  // x = (b2-b1)/(a1-a2)
        const auto y = side.getA() * x + side.getB();  // y = a2*x+b2
        intersectPoint = ExternalPoint{x, y};
    } else if (ray.isVertical() && !side.isVertical()) {
        const auto x = ray.getStart().x;
        const auto y = side.getA() * x + side.getB();
        intersectPoint = ExternalPoint{x, y};
    } else if (!ray.isVertical() && side.isVertical()) {
        const auto x = side.getStart().x;
        const auto y = ray.getA() * x + ray.getB();
        intersectPoint = ExternalPoint{x, y};
    } else {
        return false;
    }
    return side.isInside(intersectPoint) && ray.isInside(intersectPoint);
}

bool ExternalPolygon::polygonContainsPoint(const LatLon& latLonTarget, const vector<LatLon>& latLons) {
    Builder polygonFrame;
    for (const auto& latLon : latLons) {
        polygonFrame.addVertex(ExternalPoint{latLon.lat(), latLon.lon()});
    }
    const auto polygonShape = polygonFrame.build();
    return polygonShape.contains(latLonTarget.asPoint());
}
