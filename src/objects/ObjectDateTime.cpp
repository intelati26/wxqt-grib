// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/ObjectDateTime.h"
#include <QDebug>
#include <QString>
#include <QTimeZone>
#include <vector>
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityString.h"
#include "util/UtilityTimeSunMoon.h"

string ObjectDateTime::formatHourMinute{"MM-dd HH:mm"}; // "yyyy-MM-dd HH:mm"

ObjectDateTime::ObjectDateTime()
    : dateTime{QDateTime::currentDateTimeUtc()}
{}

ObjectDateTime::ObjectDateTime(const QDateTime& dateTime)
    : dateTime{dateTime}
{}

void ObjectDateTime::addDays(int i) {
   dateTime = dateTime.addDays(i);
}

void ObjectDateTime::addHours(int i) {
    dateTime = dateTime.addSecs(i * 60 * 60);
}

void ObjectDateTime::addSeconds(int i) {
    dateTime = dateTime.addSecs(i);
}

// KEEP
// bool ObjectDateTime::isAfter(const ObjectDateTime& dt) const {
//    return dateTime > dt.get();
// }

bool ObjectDateTime::isBefore(const ObjectDateTime& dt) const {
    return dateTime < dt.get();
}

string ObjectDateTime::format(const string& s) const {
    return dateTime.toString(QString::fromStdString(s)).toStdString();
}

void ObjectDateTime::utcToLocal() {
    dateTime.setTimeSpec(Qt::UTC);
    dateTime = dateTime.toLocalTime();
}

QDateTime ObjectDateTime::get() const {
    return dateTime;
}

ObjectDateTime ObjectDateTime::fromObs(const string& time) {
    // time comes in as follows 2018.02.11 2353 UTC
    // https://en.wikipedia.org/wiki/ISO_8601
    auto returnTime = WString::strip(time);
    returnTime = WString::replace(returnTime, " UTC", "");
    returnTime = WString::replace(returnTime, ".", "");
    returnTime = WString::replace(returnTime, " ", "T") + "00.000Z";
    // time should now be as "20220225T095300.000Z"
    // text has a timezone "Z" so 2nd arg is null
    // qDebug() << "decode:" << returnTime;
    auto dateTime = QDateTime::fromString(QString::fromStdString(returnTime), "yyyyMMddThhmmss.zzzZ");
    dateTime.setTimeSpec(Qt::UTC);
    if (!dateTime.isValid()) {
        return ObjectDateTime{};
    }
    return ObjectDateTime{dateTime};
}

ObjectDateTime ObjectDateTime::fromMoonTimes(vector<int> date) {
    auto objectDateTime = ObjectDateTime{QDateTime(QDate{date[0], date[1], date[2]}, QTime{date[3], date[4], date[5]}, QTimeZone::utc())};
    objectDateTime.utcToLocal();
    return objectDateTime;
}

ObjectDateTime ObjectDateTime::decodeVtecTime(const string& timeRange) {
    const auto year = To::Int(("20" + UtilityString::parse(timeRange, "([0-9]{2})[0-9]{4}T[0-9]{4}")));
    const auto month = To::Int(UtilityString::parse(timeRange, "[0-9]{2}([0-9]{2})[0-9]{2}T[0-9]{4}"));
    const auto day = To::Int(UtilityString::parse(timeRange, "[0-9]{4}([0-9]{2})T[0-9]{4}"));
    const auto hour = To::Int(UtilityString::parse(timeRange, "[0-9]{6}T([0-9]{2})[0-9]{2}"));
    const auto minute = To::Int(UtilityString::parse(timeRange, "[0-9]{6}T[0-9]{2}([0-9]{2})"));
    auto objectDateTime = ObjectDateTime{QDateTime(QDate{year, month, day}, QTime{hour, minute, 0}, QTimeZone::utc())};
    if (!objectDateTime.get().isValid()) {
        objectDateTime = ObjectDateTime{};
        objectDateTime.addHours(1);
    }
    return objectDateTime;
}

ObjectDateTime ObjectDateTime::fromString(const string& time, const string& format) {
    auto dateTime = QDateTime::fromString(QString::fromStdString(time), QString::fromStdString(format));
    return ObjectDateTime{dateTime};
}

ObjectDateTime ObjectDateTime::parse(const string& time, const string& format) {
    auto dateTime = QDateTime::fromString(QString::fromStdString(time), QString::fromStdString(format));
    return ObjectDateTime{dateTime};
}

ObjectDateTime ObjectDateTime::getCurrentTimeInUTC() {
    return ObjectDateTime{};
}

// const auto offset = QTimeZone::systemTimeZone().offsetFromUtc(QDateTime::currentDateTime()) / 3600;
int ObjectDateTime::offsetFromUtcInSeconds() {
    const auto dateTime1 = QDateTime::currentDateTime();
    const auto dateTime2 = QDateTime{dateTime1.date(), dateTime1.time(), Qt::UTC};
    return static_cast<int>(dateTime1.secsTo(dateTime2));
}

// not UTC
string ObjectDateTime::getDateAsString(const string& format) {
    const auto currentTime = QDateTime::currentDateTime();
    return currentTime.toString(QString::fromStdString(format)).toStdString();
}

// SAVE for format string
// string ObjectDateTime::getCurrentLocalTimeAsString() {
//     return getDateAsString("yyyy-MM-dd HH:mm:ss");
// }

string ObjectDateTime::getLocalTimeAsString() {
    return getDateAsString("HH:mm:ss");
}

string ObjectDateTime::getGmtTimeForVtec() {
    const auto currentTime = QDateTime::currentDateTimeUtc();
    return currentTime.toString("yyMMddThhmm").toStdString();
}

int64_t ObjectDateTime::currentTimeMillis() {
    auto local = QDateTime::currentDateTime();
    return local.toMSecsSinceEpoch();
}

// used by UtilityHourlyOldApi.cpp
string ObjectDateTime::dayOfWeekAbbreviation(int year, int month, int day, int hour) {
    const auto localTime = QDateTime{QDate{year, month, day}, QTime{hour, 0, 0}};
    return localTime.toString("ddd").toStdString();
}

int ObjectDateTime::getYear() {
    return QDate::currentDate().year();
}

string ObjectDateTime::getYearString() {
    return To::string(QDate::currentDate().year());
}

string ObjectDateTime::getYearShortString() {
    return To::string(QDate::currentDate().year()).substr(2);
}

int ObjectDateTime::getMonth() {
    return QDate::currentDate().month();
}

int ObjectDateTime::getDay() {
    return QDate::currentDate().day();
}

int ObjectDateTime::getHour() {
    struct tm aTime;
    const time_t theTime = time(nullptr);
    // const struct tm *aTime = localtime(&theTime);
    localtime_r(&theTime, &aTime);
    const int hour = aTime.tm_hour;
    return hour;
}

int ObjectDateTime::getMinute() {
    struct tm aTime;
    const time_t theTime = time(nullptr);
    // const struct tm *aTime = localtime(&theTime);
    localtime_r(&theTime, &aTime);
    const int min = aTime.tm_min;
    return min;
}

int ObjectDateTime::getDayOfWeek() {
    // wxDateTime now = wxDateTime::Now(); // .MakeUTC().Format().c_str()
    // return now.GetDay();
    return QDate::currentDate().dayOfWeek();
}

int ObjectDateTime::getCurrentHourInUTC() {
    const auto currentTime = QDateTime::currentDateTimeUtc();
    return currentTime.time().hour();
}

bool ObjectDateTime::timeDifference(const ObjectDateTime& t1, const ObjectDateTime& t2, int m) {
    auto date = t2.get().addSecs(m * 60);
    return date > t1.get();
}

bool ObjectDateTime::isDaytime(const Site& obs) {
    // 2 element list with sunrise 1st, sunrise 2nd
    const auto sunTimes = UtilityTimeSunMoon::getSunriseSunsetFromObs(obs);

    QTime sr{0, 0, 0};
    sr = sr.addSecs(static_cast<int>(60 * sunTimes[0]));
    QTime ss{0, 0, 0};
    ss = ss.addSecs(static_cast<int>(60 * sunTimes[1]));

    const auto currentTime = QTime::currentTime();
    return !(currentTime > ss || currentTime < sr);
}

string ObjectDateTime::getTimeFromPointAsString(uint64_t sec) {
    const auto radarTime = QDateTime::fromMSecsSinceEpoch(sec * 1000);
    return radarTime.toString("hh:mm:ss").toStdString();
}

string ObjectDateTime::convertFromUTCForMetar(const string& time) {
    return time;
}

vector<string> ObjectDateTime::generateModelRuns([[maybe_unused]] const string& time1, [[maybe_unused]] int hours) {  // wxc2 has 3rd arg , QString dateStr
    // TODO FIXME
    //  QDateTime dateObject = QDateTime::fromString(time1, dateStr);
    vector<string> runs;
    //  for (int index = 1; index < 5; index++) {
    //      float timeChange = 60.0 * 60.0 * float(hours) * float(index);
    //      QDateTime newDateTime = dateObject.addSecs(-1.0 * timeChange);
    //      runs.append(newDateTime.toString(dateStr));
    //  }
    return runs;
}

string ObjectDateTime::translateTimeForHourly(const string& originalTime) {
    auto value = originalTime;
    value = WString::replace(value, "T", "-");
    const auto originalTimeComponents = WString::split(value, "-");
    const auto hour = To::Int(WString::replace(originalTimeComponents[3], ":00:00", ""));
    return hourlyDayOfWeek(originalTime) + " " + To::string(hour);
}

string ObjectDateTime::hourlyDayOfWeek(const string& originalTime) {
    auto value = originalTime;
    value = WString::replace(value, "T", "-");
    auto originalTimeComponents = WString::split(value, "-");
    const auto year = To::Int(originalTimeComponents[0]);
    const auto month = To::Int(originalTimeComponents[1]);
    const auto day = To::Int(originalTimeComponents[2]);
    const auto hour = To::Int(WString::replace(originalTimeComponents[3], ":00:00", ""));
    return ObjectDateTime::dayOfWeekAbbreviation(year, month, day, hour);
}

vector<int> ObjectDateTime::currentTimeForMoon() {
    // gtkmm
    // auto objectDateTime = local();
    // objectDateTime.addHours(18);
    // const auto year = objectDateTime.getYear();
    // const auto month = objectDateTime.getMonth();
    // const auto day = objectDateTime.getDay();
    // const auto h = objectDateTime.getHour();
    // const auto m = objectDateTime.getMinute();
    // return {year, month, day, h, m, 0};

    const auto time = ObjectDateTime{};
    const auto year = time.getYear();
    const auto month = time.getMonth();
    const auto day = time.getDay();
    const auto h = time.getHour();
    const auto m = time.getMinute();
    return {year, month, day, h, m, 0};
}

string ObjectDateTime::timeOfDayFromMinutes(double minutes) {
    const auto hours = static_cast<int>(minutes / 60);
    const auto leftOver = static_cast<int>(minutes) % 60;
    return WString::fixedLengthStringPad0(To::string(hours), 2) + ":" + WString::fixedLengthStringPad0(To::string(leftOver), 2);
}

// const auto sunrise = sun.calcSunrise();
// QTime sr{0, 0, 0};
// sr = sr.addSecs(static_cast<int>(60 * sunrise));
// return "Sunrise: " + sr.toString("hh:mm ap").toStdString() + " Sunset: " + ss.toString("hh:mm ap").toStdString();

QDebug operator<<(QDebug dbg, const ObjectDateTime &objectDateTime) {
    dbg.nospace() << objectDateTime.get().toString() << " ";
    return dbg.maybeSpace();
}
