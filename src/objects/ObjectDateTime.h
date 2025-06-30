// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTDATETIME_H
#define OBJECTDATETIME_H

#include <cstdint>
#include <string>
#include <vector>
#include <QDateTime>
#include "objects/Site.h"

using std::string;
using std::vector;

class ObjectDateTime {
public:
    ObjectDateTime();
    explicit ObjectDateTime(const QDateTime&);
    void addDays(int);
    void addHours(int);
    void addSeconds(int);
    bool isBefore(const ObjectDateTime&) const;
    void utcToLocal();
    string format(const string&) const;
    QDateTime get() const;
    static ObjectDateTime fromObs(const string&);
    static ObjectDateTime fromMoonTimes(vector<int>);
    static ObjectDateTime decodeVtecTime(const string&);
    static ObjectDateTime parse(const string&, const string&);
    static ObjectDateTime fromString(const string&, const string&);
    static ObjectDateTime getCurrentTimeInUTC();
    static int offsetFromUtcInSeconds();
    static string getLocalTimeAsString();
    static string getGmtTimeForVtec();
    static int64_t currentTimeMillis();
    static string dayOfWeekAbbreviation(int, int, int, int);
    static int getYear();
    static string getYearString();
    static string getYearShortString();
    static int getMonth();
    static int getDay();
    static int getHour();
    static int getMinute();
    static int getDayOfWeek();
    static int getCurrentHourInUTC();
    static bool timeDifference(const ObjectDateTime&, const ObjectDateTime&, int);
    static bool isDaytime(const Site&);
    static string getTimeFromPointAsString(uint64_t);
    static string convertFromUTCForMetar(const string&);
    static vector<string> generateModelRuns(const string&, int);
    static string translateTimeForHourly(const string&);
    static vector<int> currentTimeForMoon();
    static string timeOfDayFromMinutes(double);
    static string formatHourMinute;

private:
    static string getDateAsString(const string&);
    static string hourlyDayOfWeek(const string&);
    QDateTime dateTime;

};
QDebug operator<< (QDebug d, const ObjectDateTime&);

#endif  // OBJECTDATETIME_H
