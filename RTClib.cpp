// Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!

#include <RTClib.h>
#if !defined(__arm__)

#define DS1307_ADDRESS 0x68


#if 0
////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

const uint8_t daysInMonth [] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //has to be const or compiler compaints

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
        if (y >= 2000)
                y -= 2000;
        uint16_t days = d;
        for (uint8_t i = 1; i < m; ++i)
                days += pgm_read_byte(daysInMonth + i - 1);
        if (m > 2 && y % 4 == 0)
                ++days;
        return days + 365 * y + (y + 3) / 4 - 1;
}

static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
        return ((days * 24L + h) * 60 + m) * 60 + s;
}
#endif
#endif // ! __arm__

////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

#ifdef __AVR__
// 64bit time_t -> struct tm
DateTime::DateTime(time_t t) {
        gmtime_r((const time_t *)&t, &_time);
}
#endif

// 32bit time_t -> struct tm
DateTime::DateTime(int32_t t) {
        time_t x = t;
        gmtime_r((const time_t *)&x, &_time);
}

// YY/MM/DD/HH/mm/SS -> struct tm
DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
        _time.tm_year = year - 1900;
        _time.tm_mon = month;
        _time.tm_mday = day;
        _time.tm_hour = hour;
        _time.tm_min = min;
        _time.tm_sec = sec;
        _time.tm_isdst = 0;
        mktime(&_time);
}

// fat time -> struct tm
DateTime::DateTime(uint16_t fdate, uint16_t ftime) {
        /*
         * Time pre-packed for fat file system
         *
         *       bit31:25 year
         *       bit24:21 month
         *       bit20:16 day
         * ------------------------
         *       bit15:11 h
         *       bit10:5 m
         *       bit4:0 s (2 second resolution)
         */
        _time.tm_year = (1980 + ((fdate >> 9) & 0x7f)) - 1900;
        _time.tm_mon = (fdate >> 5) & 0x0f;
        _time.tm_mday = fdate & 0x1f;
        _time.tm_hour = (ftime >> 11) & 0x1f;
        _time.tm_min = (ftime >> 5) & 0x3f;
        _time.tm_sec = (ftime & 0x1f) *2;
        _time.tm_isdst = 0;
        mktime(&_time);
}

static uint8_t conv2d(const char* p) {
        uint8_t v = 0;
        if ('0' <= *p && *p <= '9')
                v = *p - '0';
        return 10 * v + *++p - '0';
}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using PSTR would further reduce the RAM footprint
// NOTE: This is a short version of strptime
DateTime::DateTime(const char* date, const char* time) {
        // sample input: date = "Dec 26 2009", time = "12:34:56"
        uint8_t m = 0;
        _time.tm_year = ((conv2d(date + 7)*100) + conv2d(date+9)) - 1900;
        switch (date[0]) {
                case 'J': m = date[1] == 'a' ? 1: m = date[2] == 'n' ? 6: 7;
                        break;
                case 'F': m = 2;
                        break;
                case 'A': m = date[2] == 'r' ? 4: 8;
                        break;
                case 'M': m = date[2] == 'r' ? 3: 5;
                        break;
                case 'S': m = 9;
                        break;
                case 'O': m = 10;
                        break;
                case 'N': m = 11;
                        break;
                case 'D': m = 12;
                        break;
        }
        _time.tm_mon = m;
        _time.tm_mday = conv2d(date + 4);
        _time.tm_hour = conv2d(time);
        _time.tm_min = conv2d(time + 3);
        _time.tm_sec = conv2d(time + 6);
        _time.tm_isdst = 0;
        mktime(&_time);
}

/**
 *
 * @return Time pre-packed for fat file system.
 * Does not work after the year 2107.
 */

time_t DateTime::FatPacked(void) const {
        return fatfs_time(&_time);
}

/**
 *
 * @return Time in seconds since 1/1/1970
 */

time_t DateTime::unixtime(void) const {
        return (mk_gmtime(&_time) + UNIX_OFFSET);
}

/**
 *
 * @return Time in seconds since 1/1/1900
 */
time_t DateTime::secondstime(void) const {
        return mk_gmtime(&_time);
}

////////////////////////////////////////////////////////////////////////////////
// RTC_DS1307 implementation

static uint8_t bcd2bin(uint8_t val) {
        return val - 6 * (val >> 4);
}

static uint8_t bin2bcd(uint8_t val) {
        return val + 6 * (val / 10);
}

static uint8_t decToBcd(uint8_t val) {
        return ( (val / 10 * 16) + (val % 10));
}

#if !defined(__arm__)

uint8_t RTC_DS1307::begin(const DateTime& dt) {
        XMEM_ACQUIRE_I2C();
        Wire.beginTransmission(DS1307_ADDRESS);
        uint8_t x = Wire.endTransmission();
        XMEM_RELEASE_I2C();
        if (x) return 0;
        return 1;
}



uint8_t RTC_DS1307::isrunning(void) {
        XMEM_ACQUIRE_I2C();
        uint8_t ss = 0;
        Wire.beginTransmission(DS1307_ADDRESS);
        WIREWRITE((uint8_t)0);
        if (!Wire.endTransmission()) {
                Wire.requestFrom(DS1307_ADDRESS, 1);
                ss = WIREREAD();
                ss = !((ss >> 7) &0x01);
        }
        XMEM_RELEASE_I2C();
        return ss;
}

uint8_t RTC_DS1307::adjust(const DateTime& dt) {
        XMEM_ACQUIRE_I2C();
        Wire.beginTransmission(DS1307_ADDRESS);
        WIREWRITE((uint8_t)0);
        WIREWRITE(bin2bcd(dt.second()));
        WIREWRITE(bin2bcd(dt.minute()));
        WIREWRITE(bin2bcd(dt.hour()));
        WIREWRITE(bin2bcd(0));
        WIREWRITE(bin2bcd(dt.day()));
        WIREWRITE(bin2bcd(dt.month()));
        WIREWRITE(bin2bcd(dt.year() - 2000));
        WIREWRITE((uint8_t)0);
        uint8_t x = Wire.endTransmission();
        XMEM_RELEASE_I2C();
        return (!x);
}

uint8_t RTC_DS1307::set(int shour, int smin, int ssec, int sday, int smonth, int syear) {
        XMEM_ACQUIRE_I2C();
        Wire.beginTransmission(DS1307_ADDRESS);
        WIREWRITE((uint8_t)0);
        WIREWRITE(decToBcd(ssec));
        WIREWRITE(decToBcd(smin));
        WIREWRITE(decToBcd(shour));
        WIREWRITE(decToBcd(0));
        WIREWRITE(decToBcd(sday));
        WIREWRITE(decToBcd(smonth));
        WIREWRITE(decToBcd(syear - 2000));
        WIREWRITE((uint8_t)0);
        uint8_t x = Wire.endTransmission();
        XMEM_RELEASE_I2C();
        return (!x);
}

// This is bad, can't report error.

DateTime RTC_DS1307::now() {
        XMEM_ACQUIRE_I2C();
        Wire.beginTransmission(DS1307_ADDRESS);
        WIREWRITE((uint8_t)0);
        Wire.endTransmission();

        Wire.requestFrom(DS1307_ADDRESS, 7);
        uint8_t ss = bcd2bin(WIREREAD() & 0x7F);
        uint8_t mm = bcd2bin(WIREREAD());
        uint8_t hh = bcd2bin(WIREREAD());
        WIREREAD();
        uint8_t d = bcd2bin(WIREREAD());
        uint8_t m = bcd2bin(WIREREAD());
        uint16_t y = bcd2bin(WIREREAD()) + 2000;
        XMEM_RELEASE_I2C();

        return DateTime(y, m, d, hh, mm, ss);
}

uint8_t RTC_DS1307::readMemory(uint8_t offset, uint8_t* data, uint8_t length) {
        XMEM_ACQUIRE_I2C();
        uint8_t bytes_read = 0;

        Wire.beginTransmission(DS1307_ADDRESS);
        WIREWRITE(0x08 + offset);
        if (!Wire.endTransmission()) {

                Wire.requestFrom((uint8_t)DS1307_ADDRESS, (uint8_t)length);
                while (Wire.available() > 0 && bytes_read < length) {
                        data[bytes_read] = WIREREAD();
                        bytes_read++;
                }
        }
        XMEM_RELEASE_I2C();
        return bytes_read;
}

uint8_t RTC_DS1307::writeMemory(uint8_t offset, uint8_t* data, uint8_t length) {
        XMEM_ACQUIRE_I2C();

        Wire.beginTransmission(DS1307_ADDRESS);
        WIREWRITE(0x08 + offset);
        uint8_t bytes_written = WIREWRITE(data, length);
        if (Wire.endTransmission()) bytes_written = -1;
        XMEM_RELEASE_I2C();
        return bytes_written;
}

Ds1307SqwPinMode RTC_DS1307::readSqwPinMode() {
        XMEM_ACQUIRE_I2C();
        int mode;

        Wire.beginTransmission(DS1307_ADDRESS);
        WIREWRITE(0x07);
        Wire.endTransmission();

        Wire.requestFrom((uint8_t)DS1307_ADDRESS, (uint8_t)1);
        mode = WIREREAD();
        XMEM_RELEASE_I2C();

        mode &= 0x93;
        return static_cast<Ds1307SqwPinMode>(mode);
}

void RTC_DS1307::writeSqwPinMode(Ds1307SqwPinMode mode) {
        XMEM_ACQUIRE_I2C();
        Wire.beginTransmission(DS1307_ADDRESS);
        WIREWRITE(0x07);
        WIREWRITE(mode);
        Wire.endTransmission();
        XMEM_RELEASE_I2C();
}


////////////////////////////////////////////////////////////////////////////////
// RTC_Millis implementation

int64_t RTC_Millis::offset = 0;

uint8_t RTC_Millis::adjust(const DateTime& dt) {
        XMEM_ACQUIRE_I2C(); // Yes, this is not I2C, but we should make this safe
        offset = dt.secondstime() - millis() / 1000;
        XMEM_RELEASE_I2C();
        return 1;
}

DateTime RTC_Millis::now() {
        return DateTime((time_t)(offset + millis() / 1000));
}

uint8_t RTC_Millis::isrunning(void) {
        return offset ? 0 : 1;
}

////////////////////////////////////////////////////////////////////////////////
//
// Add more RTC as needed.
//
////////////////////////////////////////////////////////////////////////////////
RTC_DS1307 RTC_DS1307_RTC;
RTC_Millis RTC_ARDUINO_MILLIS_RTC;
static boolean WireStarted = false;
#endif // ! __arm__

void RTCstart(void) {
#if !defined(__arm__)
        if (!WireStarted) {
                WireStarted = true;
                RTC_ARDUINO_MILLIS_RTC.begin(DateTime(__DATE__, __TIME__));
                Wire.begin();
                if (!RTC_DS1307_RTC.isrunning())
                        RTC_DS1307_RTC.adjust(DateTime(__DATE__, __TIME__));
                // Add more RTC as needed.
        }
#endif
}

void RTCset(const DateTime& dt) {
#if defined(__arm__)
        rtc_set(dt.unixtime());
#else
        RTCstart(); // Automatic.
        RTC_ARDUINO_MILLIS_RTC.adjust(dt); // Always present.
        if (RTC_DS1307_RTC.isrunning()) {
                RTC_DS1307_RTC.adjust(dt);
        }
        // Add more RTC as needed.
#endif
}

DateTime RTCnow(void) {
#if defined(__arm__)
        return DateTime(rtc_get());
#else
        RTCstart(); // Automatic.
        if (RTC_DS1307_RTC.isrunning()) return RTC_DS1307_RTC.now();
        // Add more RTC as needed.
        return RTC_ARDUINO_MILLIS_RTC.now();
#endif
}

boolean RTChardware(void) {
#if defined(__arm__)
        return true;
#else
        RTCstart(); // Automatic.
        if (RTC_DS1307_RTC.isrunning()) return true;
        // Add more RTC as needed.
        return false;
#endif
}
