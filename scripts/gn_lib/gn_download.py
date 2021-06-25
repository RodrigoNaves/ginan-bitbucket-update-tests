'''
Functions to download files necessary for Ginarn processing:
sp3
erp
clk
rnx (including transformation from crx to rnx)
'''
from datetime import datetime as _datetime
from ftplib import FTP_TLS as _FTP_TLS
from pathlib import Path as _Path
import subprocess as _sp
import requests as _rqs
import numpy as _np
import sys as _sys



def check_file_present(comp_filename, dwndir):
    '''Check if file comp_filename already present in directory dwndir'''
    
    if dwndir[-1] != '/':
        dwndir += '/'

    if comp_filename.endswith('.gz'):
        uncomp_file = _Path(dwndir+comp_filename[:-3])
    elif comp_filename.endswith('.Z'):
        uncomp_file = _Path(dwndir+comp_filename[:-2])

    if comp_filename.endswith('.crx.gz'):
        rnx_Z_file = _Path(str(uncomp_file)[:-3]+'rnx')
        if rnx_Z_file.is_file():
            print(f'File {rnx_Z_file.name} already present in {dwndir}')
            return True
    
    if uncomp_file.is_file():
        print(f'File {uncomp_file.name} already present in {dwndir}')
        present = True
    else:
        present = False
    
    return present



def check_n_download(comp_filename, dwndir, ftps, uncomp=True, remove_crx=False):
    '''Download compressed file to dwndir if not already present and optionally uncompress'''
    
    comp_file = _Path(dwndir+comp_filename)

    if dwndir[-1] != '/':
        dwndir += '/'

    if not check_file_present(comp_filename, dwndir):
        
        print(f'Downloading {comp_filename}')
        
        with open(comp_file, 'wb') as local_f:
            ftps.retrbinary(f'RETR {comp_filename}', local_f.write)
        
        if uncomp:
            _sp.run(['uncompress',f'{comp_file}'])
            # If RINEX file, need to convert from Hatanaka compression
            if comp_filename.endswith('.crx.gz'):
                crx_file = _Path(dwndir+comp_filename[:-3])
                _sp.run(['crx2rnx',f'{str(crx_file)}'])

            print(f'Downloaded and uncompressed {comp_filename}')
        else:
            print(f'Downloaded {comp_filename}')
        
        if remove_crx:
            if comp_filename.endswith('.crx.gz'):
                crx_file.unlink()



def gpsweekD(yr, doy, wkday_suff=False):
    """
    Convert year, day-of-year to GPS week format: WWWWD or WWWW
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
    dt = _datetime.strptime(f"{yr}-{doy:03d} 01","%Y-%j %H")
    
    wkday = dt.weekday() + 1

    if wkday == 7:
        wkday = 0
    
    mn, dy, hr = dt.month, dt.day, dt.hour

    if mn <= 2:
        yr = yr-1
        mn = mn+12

    JD = _np.floor(365.25*yr) + _np.floor(30.6001*(mn+1)) + dy + hr/24.0 + 1720981.5
    GPS_wk = _np.int(_np.floor((JD-2444244.5)/7.0))
    
    if wkday_suff:
        return str(GPS_wk)+str(wkday)
    else:
        return str(GPS_wk)


def dt2gpswk(dt,wkday_suff=False,both=False):
    '''
    Convert the given datetime object to a GPS week (option to include day suffix)
    '''
    yr = dt.strftime('%Y')
    doy = dt.strftime('%j')
    if not both:
        return gpsweekD(yr,doy,wkday_suff=wkday_suff)
    else:
        return gpsweekD(yr,doy,wkday_suff=False),gpsweekD(yr,doy,wkday_suff=True)


def get_install_crx2rnx(override=False,verbose=False):
    '''
    Check for presence of crx2rnx in PATH.
    If not present, download and extract to python environment PATH location.
    If override = True, will download if present or not
    '''
    if (not _Path(f'{_sys.path[0]}/crx2rnx').is_file()) or (override):

        tmp_dir = _Path('tmp')
        if not tmp_dir.is_dir():
            tmp_dir.mkdir()

        url = 'https://terras.gsi.go.jp/ja/crx2rnx/RNXCMP_4.0.8_src.tar.gz'
        rq = _rqs.get(url,allow_redirects=True)
        with open(_Path('tmp/RNXCMP_4.0.8_src.tar.gz'),'wb') as f:
            f.write(rq.content)

        _sp.run(['tar', '-xvf', 'tmp/RNXCMP_4.0.8_src.tar.gz', '-C', 'tmp'])
        cp = ['gcc','-ansi','-O2','-static','tmp/RNXCMP_4.0.8_src/source/crx2rnx.c','-o','crx2rnx']
        _sp.run(cp)
        _sp.run(['rm','-r','tmp'])
        _sp.run(['mv','crx2rnx',_sys.path[0]])
    else:
        if verbose:
            print(f'crx2rnx already present in {_sys.path[0]}')


def dates_type_convert(dates):
    '''Convert the input variable (dates) to a list of datetime objects'''
    typ_dt = type(dates)
    if  typ_dt == _datetime:
        dates = [dates]
        typ_dt = type(dates)
    elif typ_dt == _np.datetime64:
        dates = [dates.astype(_datetime)]
        typ_dt = type(dates)
    elif typ_dt == str:
        dates = [_np.datetime64(dates)]
        typ_dt = type(dates)

    if (type(dates) == list) or (type(dates) == _np.ndarray):
        dt_list = []
        for dt in dates:
            if type(dt) == _datetime:
                dt_list.append(dt)
            elif type(dt) == _np.datetime64:
                dt_list.append(dt.astype(_datetime))
            elif type(dt) == str:
                dt_list.append(_np.datetime64(dt).astype(_datetime))

    return dt_list


def download_rinex3(dates, station, dest, dwn_src='cddis', ftps=False):
    '''
    Function used to get the RINEX3 observation file from download server of choice, default: CDDIS
    '''
    # Convert input to list of datetime dates (if not already)
    dt_list = dates_type_convert(dates)

    f_pref = f'{station}_R_'
    f_suff_crx = f'0000_01D_30S_MO.crx.gz'

    # Create directory if doesn't exist:
    if not _Path(dest).is_dir():
        _Path(dest).mkdir(parents=True)

    # If ftps not provided, establish ftps, otherwise download straight from provided ftps:
    if not ftps:
        
        # Connect to chosen server
        if dwn_src=='cddis':
            
            ftps = _FTP_TLS('gdc.cddis.eosdis.nasa.gov')
            ftps.login()
            ftps.prot_p()

            for dt in dt_list:
                f = f_pref+dt.strftime('%Y%j')+f_suff_crx
                ftps.cwd(f"gnss/data/daily{dt.strftime('/%Y/%j/%yd/')}")
                check_n_download(f, dwndir=dest, ftps=ftps, uncomp=True, remove_crx=True)
                ftps.cwd('/')
    
    else:
        for dt in dt_list:
            f = f_pref+dt.strftime('%Y%j')+f_suff_crx
            check_n_download(f, dwndir=dest, ftps=ftps, uncomp=True, remove_crx=True)


def download_sp3(dates, dest, pref='igs', dwn_src='cddis', ftps=False):
    '''
    Function used to get the sp3 orbit file from download server of choice, default: CDDIS

    Input:
    dest - destination (str)
    pref - Analysis Center / product of choice (e.g. igs, igr, cod, jpl, gfz, default = igs)
    dwn_src - Download Source (e.g. cddis, ga)
    ftps - Optionally input active ftps connection object
    '''

    # Convert input to list of datetime dates (if not already)
    if (type(dates) == list) and (type(dates[0]) == _datetime):
        dt_list = dates
    else:
        dt_list = dates_type_convert(dates)

    # Create directory if doesn't exist:
    if not _Path(dest).is_dir():
        _Path(dest).mkdir(parents=True)

    # If ftps not provided, establish ftps, otherwise download straight from provided ftps:
    if not ftps:
        
        # Connect to chosen server
        if dwn_src=='cddis':
            
            ftps = _FTP_TLS('gdc.cddis.eosdis.nasa.gov')
            ftps.login()
            ftps.prot_p()

            for dt in dt_list:
                gpswk, gpswkD = dt2gpswk(dt,both=True)
                ftps.cwd(f'gnss/products/{gpswk}')
                f = f'{pref}{gpswkD}.sp3.Z'
                check_n_download(f, dwndir=dest, ftps=ftps, uncomp=True, remove_crx=True)
                ftps.cwd('/')
    
    else:
        for dt in dt_list:
            gpswk, gpswkD = dt2gpswk(dt,both=True)
            f = f'{pref}{gpswkD}.sp3.Z'
            check_n_download(f, dwndir=dest, ftps=ftps, uncomp=True, remove_crx=True)

