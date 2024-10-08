/*
 * Copyright (c) 2006 D.Ineiev <ineiev@yahoo.co.uk>
 * Copyright (c) 2020 Emeric Grange <emeric.grange@gmail.com>
 * Modified after
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
*/

#include "egm96Data.hpp"
#include <cstdint>
#include <cmath>

using u32 = uint32_t;

class EGM96 {
public:
    double drts[1301];
    double dirt[1301];

    EGM96() {
        u32 nmax2p = (2 * maxDeg) + 1;

        for (u32 n = 1; n <= nmax2p; n++) {
            drts[n] = sqrt(n);
            dirt[n] = 1 / drts[n];
        }
    }

    /*!
    * \brief Compute the geoid undulation from the EGM96 potential coefficient model, for a given latitude and longitude.
    * \param latitude: Latitude (in degrees).
    * \param longitude: Longitude (in degrees).
    * \return The geoid undulation / altitude offset (in meters).
    */
    double egm96ComputeAltitudeOffset(double lat, double lon) {
        const double rad = (180.0 / M_PI);
        return calculateGeoidUndulationAtCoordinates(lat / rad, lon / rad);
    }

private:
    static constexpr u32 coeffsCount = 65342; // size of correction and harmonic coefficients arrays (361 * 181) in the header
    static constexpr u32 maxDeg = 360; // maximum degree and orders of harmonic coefficients.
    static constexpr double WGS84GravitationalConstant = 0.3986004418e15; // in m³/s² mass of Earth’s atmosphere included
    static constexpr double WGS84DatumSurfaceEquatorialRadius = 6378137.0;

    double calculateGravitationalUndulation(double p[coeffsCount], double sinml[maxDeg + 2], double cosml[maxDeg + 2], double gr, double re) {
        double ar = WGS84DatumSurfaceEquatorialRadius / re;
        double arn = ar;
        double ac = 0;
        double a = 0;

        u32 k = 3;
        for (u32 n = 2; n <= maxDeg; n++) {
            arn *= ar;
            k++;
            double sum = p[k]*egm96Data[k][2];
            double sumc = p[k]*egm96Data[k][0];

            for (u32 m = 1; m <= n; m++) {
                k++;
                double tempc = egm96Data[k][0] * cosml[m] + egm96Data[k][1] * sinml[m];
                double temp  = egm96Data[k][2] * cosml[m] + egm96Data[k][3] * sinml[m];
                sumc += p[k] * tempc;
                sum  += p[k] * temp;
            }
            ac += sumc;
            a += sum * arn;
        }
        ac += egm96Data[1][0] + (p[2] * egm96Data[2][0]) + (p[3] * (egm96Data[3][0] * cosml[1] + egm96Data[3][1] * sinml[1]));

        // Add haco = ac/100 to convert height anomaly on the ellipsoid to the undulation
        // Add -0.53m to make undulation refer to the WGS84 ellipsoid
        return ((a * WGS84GravitationalConstant) / (gr * re)) + (ac / 100.0) - 0.53;
    }

    void computeTrigonometricSeriesForLongitude(double rlon, double sinml[maxDeg + 2], double cosml[maxDeg + 2]) {
        double a = sin(rlon);
        double b = cos(rlon);

        sinml[1] = a;
        cosml[1] = b;
        sinml[2] = 2 * b * a;
        cosml[2] = 2 * b * b - 1;

        for (u32 m = 3; m <= maxDeg; m++) {
            sinml[m] = 2 * b * sinml[m-1] - sinml[m-2];
            cosml[m] = 2 * b * cosml[m-1] - cosml[m-2];
        }
    }

    /*!
    * \param m: order.
    * \param theta: Colatitude (radians).
    * \param rleg: Normalized legendre function.
    * 
    * This subroutine computes all normalized legendre function in 'rleg'.
    * The dimensions of array 'rleg' must be at least equal to nmax+1.
    * All calculations are in double precision.
    */
    void computeNormalizedLegendreFunctions(u32 m, double theta, double rleg[maxDeg + 2]) {
        double rlnn[maxDeg + 2];
        u32 nmax1 = maxDeg + 1;
        u32 m1 = m + 1;
        u32 m2 = m + 2;
        u32 m3 = m + 3;
        u32 n, n1, n2;

        double cothet = cos(theta);
        double sithet = sin(theta);

        // compute the legendre functions
        rlnn[1] = 1;
        rlnn[2] = sithet * drts[3];
        for (n1 = 3; n1 <= m1; n1++) {
            n = n1 - 1;
            n2 = 2 * n;
            rlnn[n1] = drts[n2 + 1] * dirt[n2] * sithet * rlnn[n];
        }

        switch (m) {
            case 1:
                rleg[2] = rlnn[2];
                rleg[3] = drts[5] * cothet * rleg[2];
                break;
            case 0:
                rleg[1] = 1;
                rleg[2] = cothet * drts[3];
                break;
        }
        rleg[m1] = rlnn[m1];

        if (m2 <= nmax1) {
            rleg[m2] = drts[m1*2 + 1] * cothet * rleg[m1];
            if (m3 <= nmax1) {
                for (n1 = m3; n1 <= nmax1; n1++) {
                    n = n1 - 1;
                    if ((!m && n < 2) || (m == 1 && n < 3))
                        continue;
                    n2 = 2 * n;
                    rleg[n1] = drts[n2+1] * dirt[n+m] * dirt[n-m] * (drts[n2-1] * cothet * rleg[n1-1] - drts[n+m-1] * drts[n-m-1] * dirt[n2-3] * rleg[n1-2]);
                }
            }
        }
    }

    /*!
    * \param lat: Latitude in radians.
    * \param lon: Longitude in radians.
    * \param re: Geocentric radius.
    * \param rlat: Geocentric latitude.
    * \param gr: Normal gravity (m/sec²).
    * 
    * This subroutine computes geocentric distance to the point, the geocentric
    * latitude, and an approximate value of normal gravity at the point based the
    * constants of the WGS84(g873) system are used.
    */
    void computeGeocentricMetrics(double lat, double lon, double *rlat, double *gr, double *re) {
        const double a = 6378137.0;
        const double e2 = 0.00669437999013;
        const double geqt = 9.7803253359;
        const double k = 0.00193185265246;
        double t1 = sin(lat) * sin(lat);
        double n = a / sqrt(1.0 - (e2 * t1));
        double t2 = n * cos(lat);
        double x = t2 * cos(lon);
        double y = t2 * sin(lon);
        double z = (n * (1 - e2)) * sin(lat);

        *re = sqrt((x * x) + (y * y) + (z * z));           // compute the geocentric radius
        *rlat = atan(z / sqrt((x * x) + (y * y)));         // compute the geocentric latitude
        *gr = geqt * (1 + (k * t1)) / sqrt(1 - (e2 * t1)); // compute normal gravity (m/sec²)
    }

    /*!
    * \brief Compute the geoid undulation from the EGM96 potential coefficient model, for a given latitude and longitude.
    * \param lat: Latitude in radians.
    * \param lon: Longitude in radians.
    * \return The geoid undulation / altitude offset (in meters).
    */
    double calculateGeoidUndulationAtCoordinates(double lat, double lon) {
        double p[coeffsCount], sinml[maxDeg + 2], cosml[maxDeg + 2], rleg[maxDeg + 2];

        double rlat, gr, re;
        u32 nmax1 = maxDeg + 1;

        // compute the geocentric latitude, geocentric radius, normal gravity
        computeGeocentricMetrics(lat, lon, &rlat, &gr, &re);
        rlat = (M_PI / 2) - rlat;

        for (u32 j = 1; j <= nmax1; j++) {
            u32 m = j - 1;
            computeNormalizedLegendreFunctions(m, rlat, rleg);
            for (u32 i = j ; i <= nmax1; i++) {
                p[(((i - 1) * i) / 2) + m + 1] = rleg[i];
            }
        }
        computeTrigonometricSeriesForLongitude(lon, sinml, cosml);
        return calculateGravitationalUndulation(p, sinml, cosml, gr, re);
    }
};
