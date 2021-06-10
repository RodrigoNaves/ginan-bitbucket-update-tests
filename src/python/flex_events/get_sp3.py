'''
Get a daily sp3 file (15 min increments) searching CDDIS server.

Usage:
    python3 get_rinex3.py SSSSSSSSS YYYY MM DD
    OR
    python3 get_rinex3.py SSSSSSSSS YYYY DDD

This will get the sp3 file for station 'SSSSSSSSS' for the date YYYY (year)
and either MM (month) DD (day) OR DDD (day-of-year [doy])

This will be saved in directory: 'sp3_files'

Date: 2020-10-22 13:45 
Author: Ronald Maj 
'''
import argparse
import wget
import numpy as np
#import sys
import subprocess
from datetime import datetime, timedelta
from pathlib import Path
from get_rinex3 import check_year, check_monthday, check_doy

def gpsweekD(yr,doy):
    """
    Convert year, day-of-year to GPS week format: WWWWD
    Based on code from Kristine Larson's gps.py
    https://github.com/kristinemlarson/gnssIR_python/gps.py
    
    Input:
    yr - year (int)
    doy - day-of-year (int)

    Output:
    GPS Week in WWWWD format - weeks since 7 Jan 1980 + day of week number (str)
    """

    # Set up the date and time variables
    yr = int(yr)
    doy = int(doy)
    dt_str = f"{yr}-{doy} 01"
    dt = datetime.strptime(dt_str,"%Y-%j %H")
    
    wkday = dt.weekday() + 1

    if wkday == 7:
        wkday = 0
    
    mn, dy = dt.month, dt.day
    hr = dt.hour
    
    if mn <= 2:
        yr = yr-1
        mn = mn+12

    A = np.floor(365.25*yr)
    B = np.floor(30.6001*(mn+1))
    C = hr/24.0
    JD = A + B + dy + C + 1720981.5
    GPS_wk = np.floor((JD-2444244.5)/7.0)
    GPS_wk = np.int(GPS_wk)
    
    return str(GPS_wk)+str(wkday)


def download_sp3_cddis(filename, yr, doy):
    # Download file from CDDIS
    
    # GPS Week + Day
    gpswkD = gpsweekD(yr,doy)
    # Build URL
    begin_url = 'ftps://gdc.cddis.eosdis.nasa.gov/gnss/products/'#WWWWD/igsWWWWD.sp3.Z
    mid_url = f'{gpswkD[:-1]}/'
    url = begin_url + mid_url + filename
    
    # Get the file from the ftps server
    subprocess.run(["wget","--no-check-certificate","--ftp-user","anonymous","--ftp-password","anonemailcom", url])
    return filename 


def get_sp3(yr, doy, dest, ac='igs'):
    '''
    Function used to get the sp3 orbit file from the CDDIS server
    Will search first for an IGS final product, if not present, then search for IGS rapid

    Input:
    yr - Year (int)
    doy - Day-of-year (int)
    dest - destination (str)
    ac - Analysis Center of choice (e.g. igs, cod, jpl, gfz, esa, etc. default = igs)
    '''
    # Filename we are looking for:
    gpswkD = gpsweekD(yr,doy)
    filename = f'{ac}{gpswkD}.sp3.Z'  
    sp3_files = dest

    # Check if the sp3 file already exists:
    if Path(f"{sp3_files}/{filename[:-2]}").is_file():
        print('\nsp3 file already exists')
        print(f"{sp3_files}/{filename[:-2]}")
        return
    else:
        # Looks for final product first -- AC of choice, then IGS rapid if choice unavailable
        # Try download from CDDIS:
        try:
            try:
                Z_file = download_sp3_cddis(filename, yr, doy)
            except:
                filename = f'igr{gpswkD}.sp3.Z'    
                Z_file = download_sp3_cddis(filename, yr, doy)
        except:
            print('Please try a different day')
            return

        # Uncompress and move the .sp3 to appropriate directory
        subprocess.run(["uncompress", Z_file])

        # Move the file to directory sp3_files directory
        # Check that the download directory exists, if not create it
        if not Path.exists(Path(f'{sp3_files}')):
            Path.mkdir(Path(f'{sp3_files}')) 
        subprocess.run(["mv",filename[:-2],f"{sp3_files}"])

        # Check download and extraction was successful
        if Path(f"{sp3_files}/{filename[:-2]}").is_file():
            print('\nsp3 file sucessfully downloaded to')
            print(f"{sp3_files}/{filename[:-2]}")
        else:
            print('->->-sp3 file missing - did not download or extract correctly---')


if __name__ == "__main__":

    # Introduce command line parser
    parser = argparse.ArgumentParser(
        description = 'Get a daily sp3 file (15 min increments) searching CDDIS servers.'
        )
    
    parser.add_argument("year",
        help = "Year in YYYY format"
    )
    
    parser.add_argument("doy",
        help = "Day-of-year in DDD format - include leading zero if doy < 100"
    )
    
    parser.add_argument("-md", "--month_day", action="store_true",
        help = "Option to replace doy with month_day input with format MM-DD"
    )   

    parser.add_argument("-multiday", "--multiday",
        help = """Option to download multiple days. Follow with the end date.
        e.g. -multiday 2020-356 (if -md option selected: 2020-12-21)
        """
    )   

    parser.add_argument("-dest","--destination",
    help='Option to choose destination directory, e.g. "-dest downloads/sp3"'
    )

    # Get command line args:
    args = parser.parse_args()

    # Check that inputs are the correct format:
    year = check_year(args)
    
    # If md flag was selected, convert Month-Day input to DOY
    # Otherwise, just check that DOY is correct format
    if args.month_day:
        mn_dy = check_monthday(args)
        month = mn_dy[:2]
        day = mn_dy[3:]
        dt_str = f'{year}-{month}-{day}'
        dt = datetime.strptime(dt_str,"%Y-%m-%d")
        doy = dt.strftime("%j")         
    else:
        doy = check_doy(args)
    
    if args.destination:
        dest = args.destination
    else:
        dest = 'sp3_files'

    # If the multi-day option was selected, get end date:
    if args.multiday:
        if args.month_day:
            dt_start = dt
            dt_end = datetime.strptime(args.multiday,"%Y-%m-%d")
        else:
            dt_start = datetime.strptime(f'{year}-{doy}',"%Y-%j")
            dt_end = datetime.strptime(args.multiday,"%Y-%j")

        date_list = [dt_start + timedelta(days=x) for x in range(0, (dt_end-dt_start).days)]

        for date in date_list:
            year = date.strftime("%Y")
            doy = date.strftime("%j")
            get_sp3(year, doy, dest)
    else:   
        # Download, extract, and convert file to create sp3 file:
        get_sp3(year, doy, dest)