'''
Get a daily RINEX 3.0 file (30 sec increments) searching CDDIS, UNAVCO and GA dtp servers.

Usage:
    python3 get_rinex3.py SSSSSSSSS YYYY MM DD 
    OR
    python3 get_rinex3.py SSSSSSSSS YYYY DDD

This will get the RINEX3 file for station 'SSSSSSSSS' for the date YYYY (year)
and either MM (month) DD (day) OR DDD (day-of-year [doy])

This will be saved in directory: 'rinex3_files'

Date: 2020-09-14 13:55 
Author: Ronald Maj 
'''
import argparse
import subprocess
from datetime import datetime
from pathlib import Path

#import numpy as np
import wget

#import sys



def download_from_cddis(filename, yr, doy):
    # Download file from CDDIS
    
    begin_url = 'ftps://gdc.cddis.eosdis.nasa.gov/gnss/data/daily/'
    mid_url =f'{yr}/{doy}/{yr[-2:]}d/'
    url = begin_url + mid_url + filename
    subprocess.run(["wget","--ftp-user","anonymous","--ftp-password","anonemailcom", url])
    return filename 


def download_from_unavco(filename, yr, doy):
    # Download file from UNAVCO
    
    begin_url = 'ftp://data-out.unavco.org/pub/rinex3/obs/'
    mid_url =f'{yr}/{doy}/'
    url = begin_url + mid_url + filename
    
    return wget.download(url)  


def download_from_ga(filename, yr, doy):
    # Download file from GA
    
    begin_url = 'ftp://ftp.ga.gov.au/geodesy-outgoing/gnss/data/daily/'
    mid_url =f'{yr}/{yr[-2:]}{doy}/'
    url = begin_url + mid_url + filename
    
    return wget.download(url)  


def get_rinex(yr, doy, station, dest):
    # Only looks for daily MO crx files
    # Looks through three different data sources: CDDIS, UNAVO, GA

    filename = f'{station}_R_{yr}{doy}0000_01D_30S_MO.crx.gz'  

    # Try download from CDDIS, UNAVCO, GA:
    gz_file = download_from_cddis(filename, yr, doy)

    if not Path(gz_file).is_file():
        print(f'{filename} not available at CDDIS')
        try:
            gz_file = download_from_unavco(filename, yr, doy)
        except: 
            
            if not Path(gz_file).is_file():
                print(f'{filename} not available at UNAVCO')
                try: 
                    gz_file = download_from_ga(filename, yr, doy)
                except:
                    if not Path(gz_file).is_file():
                        print(f'{filename} notavailable at GA')
                        print('\nFile not available at any of the repositories')
                        print('Please try a different station or day')
                        return

    # Unzip and convert to RINEX 3.0 then remove the .crx
    subprocess.run(["gzip", "-d", gz_file])
    #subprocess.run(["./CRX2RNX",f"{station}_R_{yr}{doy}0000_01D_30S_MO.crx"])
    #subprocess.run(["rm",f"{station}_R_{yr}{doy}0000_01D_30S_MO.crx"])

    # Move the file to directory rinex3_files directory
    # Check that the download directory exists, if not create it
    rinex3_files = dest
    if not Path.exists(Path(rinex3_files)):
        Path.mkdir(Path(rinex3_files)) 
    subprocess.run(["mv",f"{station}_R_{yr}{doy}0000_01D_30S_MO.crx",f"{rinex3_files}"])

    # Check download and extraction was successful
    if Path(f"{rinex3_files}/{station}_R_{yr}{doy}0000_01D_30S_MO.crx").is_file():
        print('\nRINEX file sucessfully downloaded to')
        print(f"{rinex3_files}/{station}_R_{yr}{doy}0000_01D_30S_MO.crx")
    else:
        print('->->-RINEX file missing - did not download or extract correctly---')


def check_station(args):
    # Ensure the station name has 9 characters
    if len(args.station) == 9:
        return args.station
    else:
        print('->->-> Station names must be new RINEX3 9 character name including country code---')


def check_year(args):
    # Ensure the year argument is 4 characters long
    if len(args.year) == 4:
        year = args.year
    else:
        print('->->-> Year must be 4 characters long: YYYY')
    return year


def check_monthday(args):
    # Ensure the month-day argument is 5 characters long
    if len(args.doy) == 5:
        mn_dy = args.doy
    else:
        print('->->-> month-day must be 5 characters long: MM-DD')
    return mn_dy


def check_doy(args):
    # Ensure the doy argument is 4 characters long
    if len(args.doy) == 3:
        doy = args.doy
    else:
        print('->->-> day-of-year (doy) must be 3 characters long: DDD')
    return doy    



if __name__ == "__main__":

    try:
        # Introduce command line parser
        parser = argparse.ArgumentParser(
            description = 'Get a daily RINEX 3.0 file (30 sec increments) searching CDDIS, UNAVCO and GA dtp servers.'
            )
        
        # Command line function arguments
        parser.add_argument("station", 
            help = "GPS station name - must be new RINEX3 format - 9 characters"
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

        parser.add_argument("-dest","--destination",
         help = 'Option to input path to save file to, e.g. "-dest downloads/rinex3"'
        )

        # Get command line args:
        args = parser.parse_args()

        # Check that inputs are the correct format:
        station = check_station(args)
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
            destination = args.destination
        else:
            destintation = 'rnx3_files'

        # Check if the RINEX file already exists:
        if Path(f"rinex3_files/{station}_R_{year}{doy}0000_01D_30S_MO.crx").is_file():
            print('\nRINEX file already exists')
            print(f"{station}_R_{year}{doy}0000_01D_30S_MO.crx")
        else:
            # otherwise Download, extract, and convert file to create RINEX3 file:
            get_rinex(year, doy, station, destination)
            #sys.stdout.write(f'{station}-{year}-{doy}')
    
    except IndexError:
        print('->->-Need to specify date as arguments: station name "SSSSSSSSS" followed by "YYYY MM DD" or "YYYY DDD"')
