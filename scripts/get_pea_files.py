'''
Get the necessary files for running the PEA from CDDIS, 
given a station, date and download location

Input:
station - IGS station in RINEX3 style format - str
year - year in YYYY format - str
date - date in DDD day-of-year format (or MM-DD format when -md option selected) - str
dwndir - download location - str

Output:
SNX, SP3, clk_30s, RINEX3 obs, and RINEX nav files

 --- TO DO ---
To clean up code need to:
    - check rapid file presence before connection / download attempt
    - move to faster ftp/ftps/rclone from wget (partially done)
    - consolidate and reduce code length by having general functions for
        downloading 'product' and 'data' files
    - output dict of files to pass into create_yaml_PPP.py function (done for rapid)
 -------------
Ronald Maj
2020-11-14 14:23
'''

import argparse
from datetime import datetime, timedelta

from ftplib import FTP_TLS as _FTP_TLS
from pathlib import Path as _Path
import subprocess as _sp
import urllib.request as _rqs
import pandas as _pd
import numpy as _np
import sys as _sys



import matplotlib.pyplot as _plt
import matplotlib.dates as mdates
from datetime import timedelta as _timedelta

from gn_lib.gn_datetime import j20002datetime
from gn_lib.gn_io.rinex import _read_rnx, _rnx_pos
from gn_lib.gn_io.sp3 import read_sp3 as _read_sp3
from gn_lib.gn_transform import xyz2llh_heik, llh2rot
from gn_lib.gn_download import check_n_download_url, dates_type_convert, download_rinex3, download_sp3


def dwn_rapid_product_files(dt, dwndir):
    '''
    Based on the datetime (dt) given, find the rapid IGS version of the 
    set of product files that are needed by the pea.
    If date given is beyond most recent available files, most recent
    files will be returned

    Input
    dt - datetime of interest (usually current datetime)

    Output
    dict of necessary files
    '''
    # Convert year and doy to GPS week (and GPS week + weekday)
    year = dt.year
    doy = int(dt.strftime('%j'))
    gpswk = gpsweekD(year,doy,False)
    gpswkD = gpsweekD(year,doy,True)

    # Output dictionary - with default outputs
    out_dict = {}
    z_file_clk = f'igr{gpswkD}.clk.Z'
    z_file_sp3 = f'igr{gpswkD}.sp3.Z'
    z_file_snx = f'gfz{gpswkD}.snx.Z'
    z_file_erp = f'igr{gpswkD}.erp.Z'
    out_dict['clk_file'] = z_file_clk
    out_dict['sp3_file'] = z_file_sp3
    out_dict['snx_file'] = z_file_snx
    out_dict['erp_file'] = z_file_erp
    out_dict['clk_date'] = dt
    out_dict['snx_date'] = dt

    # Check if files are already present
    z_files = [z_file_clk, z_file_sp3, z_file_snx, z_file_erp]
    z_label = ['clk', 'sp3', 'snx', 'erp']
    z_present = {}
    print(f'\nChecking files for {dt.strftime("%Y-DOY-%j")}')
    for z,label in zip(z_files,z_label):
        z_present[label] = check_file_present(z, dwndir)
    if sum(z_present.values()) == 4:
        print(f'\nFiles {z_files} already present in {dwndir}\n')
        return out_dict


    print(f'\nDownloading rapid versions of files for {dt.strftime("%Y-DOY-%j")}')
    # Connect to CDDIS server
    print('\nConnecting to CDDIS server...')
    ftps = FTP_TLS('gdc.cddis.eosdis.nasa.gov')
    ftps.login()
    ftps.prot_p()
    print('Connected.')
    
    # Check products:
    ftps.cwd('gnss/products')


    # clk file download
    if z_present['clk']:
        pass
    else:
        z_file_clk = f'igr{gpswkD}.clk.Z'
        print('\nclk file\n')
        
        if check_file_present(z_file_clk, dwndir):
            ftps.cwd(gpswk)
            mr_files = ftps.nlst()
            # clock files
            mr_clk_file = [f for f in mr_files if f.startswith(f'igr{gpswkD}.clk')]
            clk_file = mr_clk_file[0]
            out_dict['clk_file'] = clk_file
            out_dict['clk_date'] = dt

        else:
            # Week of choice (most recent - mr):
            print(f'\nSearching for {z_file_clk}')
            ftps.cwd(gpswk)
            mr_files = ftps.nlst()

            # clock files
            mr_clk_file = [f for f in mr_files if f.startswith(f'igr{gpswkD}.clk')]

            # If too recent, clock files may not be present. Go back until clock files found
            if mr_clk_file == []:
                while mr_clk_file == []:
                    print(f'File {z_file_clk} too recent')
                    c_clk_files = [f for f in mr_files if f.endswith(f'clk.Z')]

                    if c_clk_files == []:
                        GPS_day = int(gpswkD[-1])
                        dt -= timedelta(days=1+GPS_day)
                    else:
                        dt -= timedelta(days=1)

                    year = dt.year
                    doy = int(dt.strftime('%j'))
                    gpswk = gpsweekD(year,doy,False)
                    gpswkD = gpsweekD(year,doy,True)
                    
                    z_file_clk = f'igr{gpswkD}.clk.Z'
                    print(f'Searching for {z_file_clk}')
                    
                    ftps.cwd('../'+gpswk)
                    mr_files = ftps.nlst()
                    mr_clk_file = [f for f in mr_files if f.startswith(f'igr{gpswkD}.clk')]
            clk_file = mr_clk_file[0]
            out_dict['clk_file'] = clk_file
            out_dict['clk_date'] = dt
            
            # Check if present, otherwise download clk rapid file:
            check_n_download(clk_file, dwndir, ftps)


    # sp3 file download
    if z_present['sp3']:
        pass
    else:
        print('\nsp3 file\n')

        if z_present['clk']:
            ftps.cwd(gpswk)
            mr_files = ftps.nlst()

        mr_sp3_file = [f for f in mr_files if f.startswith(f'igr{gpswkD}.sp3')]
        sp3_file = mr_sp3_file[0]
        out_dict['sp3_file'] = sp3_file
        
        # Check if present, otherwise download sp3 rapid file:
        check_n_download(sp3_file, dwndir, ftps)

    # erp file download
    if z_present['erp']:
        pass
    else:
        print('\nerp file\n')

        if z_present['clk'] & z_present['sp3']:
            ftps.cwd(gpswk)
            mr_files = ftps.nlst()

        mr_erp_file = [f for f in mr_files if f.startswith(f'igr{gpswkD}.erp')]
        erp_file = mr_erp_file[0]
        out_dict['erp_file'] = erp_file
        
        # Check if present, otherwise download erp rapid file:
        check_n_download(erp_file, dwndir, ftps)

    # snx file download
    if z_present['snx']:
        pass
    else:
        # Most recent sinex file
        print('\nsnx file')
        z_file_snx = f'gfz{gpswkD}.snx.Z'
        if check_file_present(z_file_snx, dwndir):
            out_dict['snx_file'] = z_file_snx
            out_dict['snx_date'] = dt
        else:
            print(f'\nSearching for {z_file_snx}')
                
            if z_present['clk'] & z_present['sp3'] & z_present['erp']:
                ftps.cwd(gpswk)
                mr_files = ftps.nlst()

            mr_snx_file = [f for f in mr_files if f.startswith(f'gfz{gpswkD}.snx')]
            
            if mr_snx_file == []:
                while mr_snx_file == []:
                    print(f'File {z_file_snx} too recent')
                    c_snx_files = [f for f in mr_files if f.endswith(f'snx.Z')]

                    if c_snx_files == []:
                        print(f'No snx files found in GPS week {gpswk}')
                        GPS_day = int(gpswkD[-1])
                        dt -= timedelta(days=1+GPS_day)
                        print(f'Moving to GPS week {int(gpswk) - 1}')
                    else:
                        dt -= timedelta(days=1)

                    year = dt.year
                    doy = int(dt.strftime('%j'))
                    gpswk = gpsweekD(year,doy,False)
                    gpswkD = gpsweekD(year,doy,True)      
                        
                    z_file_snx = f'gfz{gpswkD}.snx.Z'
                    print(f'Searching for {z_file_snx}')

                    ftps.cwd('../'+gpswk)
                    mr_files = ftps.nlst()
                    mr_snx_file = [f for f in mr_files if f.startswith(f'gfz{gpswkD}.snx')]
            snx_file = mr_snx_file[0]
            out_dict['snx_file'] = snx_file
            out_dict['snx_date'] = dt
            
            # Check if present, otherwise download snx file:
            check_n_download(snx_file, dwndir, ftps)

    print('\n')

    return out_dict


def download_daily(f, yr, doy):
    '''
    Download file from the daily subfolder in either GA or CDDIS server

    In this case it will be either the observation or navigation file
    (obs_file, nav_file)

    Input:
    f   - filename to be downloaded - str
    yr  - year of interest          - str
    doy - day-of-year of interest   - str

    Output:
    Download the file to the pwd

    '''
    # Convert to datetime format:
    dt = datetime.strptime(yr + ' ' + doy, '%Y %j')

    # If the date of interest is before 2018-09-05
    # GA server will not have RNX3 files available
    # therefore just download from cddis
    dt_rnx3st = datetime.strptime('2018-248','%Y-%j')
    
    if dt < dt_rnx3st:
        # Download the file from CDDIS daily subfolder:
        download_daily_cddis(f, yr, doy)
    else:
        # Try to download from GA server first, then
        # if that fails, try CDDIS.
        
        # Establish ftp connection
        ftp = FTP('ftp.data.gnss.ga.gov.au')
        ftp.login()
        ftp.cwd(f'daily/{yr}/{doy}')\

        # Save contents of directory:
        f_list = []
        ftp.retrlines('NLST',f_list.append)

        # If in GA server, download, otherwise CDDIS
        if f in f_list:
            print(f'==> Downloading {f} from Geoscience Australia server')
            with open(f,'wb') as local_f:
                ftp.retrbinary(f'RETR {f}', local_f.write)
            print(f'\n Complete \n\n')
        else:
            download_daily_cddis(f, yr, doy)

    return f


def get_pea_product_files(dates, station, dest, trop_vmf3 = False, rapid = False, out_dict = None):
    '''
    Download necessary pea files for station and date/s provided
    '''
    dt_list = dates_type_convert(dates)

    # Output dict for the files that are downloaded
    if not out_dict:
        out_dict = {
            'atxfiles':['igs14.atx'],
            'blqfiles':['OLOAD_GO.BLQ']
        }
        out_dict['snxfiles'] = []
        out_dict['sp3files'] = []
        out_dict['navfiles'] = []
        out_dict['clkfiles'] = []
        out_dict['rnxfiles'] = []
        out_dict['erpfiles'] = []


    # Get the ATX file if not present already:
    if not _Path(dest + '/products/igs14.atx').is_file():
        
        if not _Path(dest+'/products').is_dir():
            _Path(dest+'/products').mkdir(parents=True)

        url = 'https://files.igs.org/pub/station/general/igs14.atx'
        check_n_download_url(url,dwndir=dest+'/products')


    # Get the BLQ file if not present already:
    if not _Path(dest + '/products/OLOAD_GO.BLQ').is_file():
        url = 'https://peanpod.s3-ap-southeast-2.amazonaws.com/pea/examples/EX03/products/OLOAD_GO.BLQ'
        check_n_download_url(url,dwndir=dest+'/products')

    # For the troposphere, have two options: gpt2 or vmf3. If flag is set to True, download 6-hourly trop files:
    if trop_vmf3:

        # If directory for the Tropospheric model files doesn't exist, create it:
        if not _Path(dest + '/products/grid5').is_dir():
            _Path(dest + '/products/grid5').mkdir(parents=True)

        for dt in dt_list:
            year = dt.strftime('%Y')
            # Create urls to the four 6-hourly files associated with the tropospheric model
            begin_url = f'https://vmf.geo.tuwien.ac.at/trop_products/GRID/5x5/VMF3/VMF3_OP/{year}/'
            f_begin = 'VMF3_' + dt.strftime('%Y%m%d') + '.H'
            urls = [ begin_url+f_begin+en for en in ['00','06','12','18'] ]
            urls.append(begin_url+'VMF3_' + (dt+timedelta(days=1)).strftime('%Y%m%d') + '.H00')

            # Run through model files, downloading if they are not in directory
            for url in urls:
                if not _Path(dest + f'/products/grid5/{url[-17:]}').is_file():
                    check_n_download_url(url,dwndir=dest+'/products/grid5')
    else:
        # Otherwise, check for GPT2 model file or download if necessary:
        if not _Path(dest + '/products/gpt_25.grd').is_file():
            url = 'https://peanpod.s3-ap-southeast-2.amazonaws.com/pea/examples/EX03/products/gpt_25.grd'
            check_n_download_url(url,dwndir=dest+'/products')
    
    dt = datetime.strptime(f'{year} {doy} 00','%Y %j %H')
    
    # If rapid versions of files desired, find the most recent files that match input date:
    if rapid:
        prod_files = dwn_rapid_product_files(dt, dest)
        
        # Change year and doy to the date of file actually downloaded (if input is too recent)
        dt = prod_files['clk_date']
        year = str(dt.year)
        doy = dt.strftime('%j')
        # Convert year and doy to GPS week (and GPS week + weekday)
        gpswk = gpsweekD(year,doy,False)
        gpswkD = gpsweekD(year,doy,True)
    
    else:
        # Convert year and doy to GPS week (and GPS week + weekday)
        gpswk = gpsweekD(year,doy,False)
        gpswkD = gpsweekD(year,doy,True)

        # Define files to be downloaded from the products subfolder on CDDIS server
        Z_filenames = {
            'snx_file':f'igs{year[2:]}P{gpswk}.snx.Z',
            'clk_file':f'igs{gpswkD}.clk_30s.Z',
            'sp3_file':f'igs{gpswkD}.sp3.Z',
            'erp_file':f'igs{gpswkD}.erp.Z'
        }

        # Download, uncompress and move product files to chosen directory
        for f in Z_filenames.values():
            
            if Path(dwndir+'/products/'+f[:-2]).is_file():
                continue
            else:
                download_prod_cddis(f, year, doy, dwndir)
    
    # Define files to download from the daily data subfolder on CDDIS server
    gz_filenames = {
        'obs_file':f'{station}_R_{year}{doy}0000_01D_30S_MO.crx.gz',
        'nav_file':f'BRDC00IGS_R_{year}{doy}0000_01D_MN.rnx.gz'
    }
    
    # Download, extract and move obs and nav files to chosen directory
    for f in gz_filenames.values():
        # If the file already exists, move to the next file
        if Path(dest+'/products/'+f[:-3]).is_file():
            continue
        else:            
            
            # If dealing with the observation file, it will be a CRX file, so needs to be converted:
            if f[-9:-7] == 'MO':
                
                # If the RNX file is already in the directory, continue to next one
                if Path(dest+'/data/'+f[:-6]+'rnx').is_file():
                    continue
                
                # Otherwise download
                else:
                    download_daily(f, year, doy)

                    # If the directory doesn't exist, make it
                    if not Path(dest + '/data').is_dir():
                        Path(dest + '/data').mkdir(parents=True)
                    
                    # If the CRX2RNX file doesn't exits, get and install it
                    if not Path('crx2rnx').is_file():
                        get_install_crx2rnx()
                    
                    subprocess.run(['gzip','-d',f])
                    subprocess.run(['./crx2rnx',f[:-3]])
                    subprocess.run(['rm',f[:-3]])
                    subprocess.run(['mv',f[:-6]+'rnx',dwndir+'/data/'])

            # Otherwise, it will be a NAV file that can be placed in products folder:
            else:
                # If the RNX file is already in the directory, continue to next one
                if Path(dwndir+'/products/'+f[:-6]+'rnx').is_file():
                    continue
                else:
                    # Download the file from daily data subfolder
                    download_daily(f, year, doy)            
                    subprocess.run(['gzip','-d',f])
                    subprocess.run(['mv',f[:-3],dwndir+'/products/'])

    # If rapid, download also create dict of files to use in create_yaml_PPP.py
    if rapid:

        for key in prod_files.keys():
            if key.endswith('_file'):
                out_dict[f'{key[:3]}files'].append(prod_files[key][:-2])
    
    else:

        for key in Z_filenames.keys():
            out_dict[f'{key[:3]}files'].append(Z_filenames[key][:-2])


    for key in gz_filenames.keys():
        if key == 'nav_file':
            out_dict['navfiles'].append(gz_filenames[key][:-3])
        elif key == 'obs_file':
            out_dict['rnxfiles'].append(f'{gz_filenames[key][:-6]}rnx')
    
    return out_dict,dt




'''
Information from: 
https://cddis.nasa.gov/Data_and_Derived_Products/GNSS/RINEX_Version_3.html


d = Hatanaka-compressed observation data
f = Beidou navigation message data
g = GLONASS navigation message data
h = SBAS payload navigation message data
l = GALILEO navigation message data
m = meteorological data
n = GPS navigation message data
o = observation data
p = mixed GNSS navigation message data
q = QZSS navigation message data
s = observation summary files (extracted from RINEX header)

Information from: 
ftp://ftp.igs.org/pub/data/format/rinex304.pdf (pg. 56)

GO - GPS Obs.,
RO - GLONASS Obs.
EO - Galileo Obs.
JO - QZSS Obs.
CO - BDS Obs.
IO – IRNSS Obs.
SO - SBAS Obs.

MO - Mixed Obs.

CN - BDS Nav.
RN - GLONASS Nav.
SN - SBAS Nav.
IN – IRNSS Nav.
EN - Galileo Nav.
GN - Nav. GPS
JN - QZSS Nav.
MM - Meteorological Observation

MN - Nav. All GNSS Constellations)

'''


daily_opts = {
    'MO':'d',
    'CN':'f',
    'RN':'g',
    'SN':'h',
    'IN':'i',
    'EN':'l',
    'MM':'m',
    'GN':'n',
    'MN':'p',
    'JN':'q',
}



if __name__ == "__main__":

    # Introduce command line parser
    parser = argparse.ArgumentParser(
        description = 'Get snx, clk, sp3, obs and nav files from CDDIS necessary for running pea'
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

    parser.add_argument("dwndir", 
        help = "Download directory"
    )  
    
    parser.add_argument("-md", "--month_day", action="store_true", 
        help = "Option to replace doy with month_day input with format MM-DD"
    )   

    parser.add_argument("-vmf3", "--vmf3_flag", action="store_true", 
        help = "Option to get VMF3 files for the troposphere"
    )   

    parser.add_argument("-rapid", "--rapid_dwn", action = "store_true",
        help = "Option to download rapid files instead. Set doy to current day to find most recent rapid files"
    )

    # Get command line args:
    args = parser.parse_args()

    # Check that inputs are the correct format:
    station = check_station(args)
    year = check_year(args)

    # Download directory:
    dwndir = args.dwndir
    
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
    
    # Get the VMF3 model files flag - if True will get 6 hourly files from UT-Wien servers
    vmf3_flag = args.vmf3_flag

    # Get the rapid files download flag
    rapid = args.rapid_dwn
    
    # Get the pea files:
    if rapid:
        if vmf3_flag:
            out = get_pea_files(station, year, doy, dwndir, trop_vmf3 = True, rapid = rapid)
        else:
            out = get_pea_files(station, year, doy, dwndir, rapid = rapid)
    else:
        if vmf3_flag:
            get_pea_files(station, year, doy, dwndir, trop_vmf3 = True)
        else:
            get_pea_files(station, year, doy, dwndir)