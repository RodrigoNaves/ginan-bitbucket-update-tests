'''
NEEDS TO BE RE-WRITTEN TO USE NEW DIRECTORY STRUCTURE (pull functions from gn_io.gn_download)

Given a station and time range, find flex events.

With flex events it is important to remember only GPS Block IIR-M and onwards have this capability
Therefore, we need to choose GPS satellites that can actually have flex power events
'''
import sys as _sys
import argparse
import pandas as _pd

import numpy as _np
from numpy.core.umath_tests import inner1d

import matplotlib.pyplot as _plt
import matplotlib.dates as mdates
from datetime import timedelta as _timedelta

from pathlib import Path as _Path

from read_metadata import df_sat_info, svn_prn_dates


from gn_lib.gn_io.rinex import _read_rnx, _rnx_pos
from gn_lib.gn_io.sp3 import read_sp3 as _read_sp3
from gn_lib.gn_download import gpsweekD, download_rinex3, download_sp3
from gn_lib.gn_datetime import j20002datetime
from gn_lib.gn_transform import xyz2llh_heik, llh2rot


def load_rnxs(rnxs):
    '''
    rnxs: List of paths to RINEX files to load
    '''
    obs = _read_rnx(rnxs[0])
    if len(rnxs)>1:
        for rnx in rnxs[1:]:
            obs_add = _read_rnx(rnx)
            obs = obs.combine_first(obs_add)
    return obs


def load_sp3s(sp3s, rec_loc=None, add_nad=True, add_el=True, add_az=True, add_dist=True):
    '''
    sp3s: List of paths to SP3 files to load
    rec_loc: The receiver location in XYZ coords
    add_*: If rec_loc given, choose which options are added
    '''
    orb = _read_sp3(sp3s[0])
    if len(sp3s)>1:
        for sp3 in sp3s[1:]:
            orb_add = _read_sp3(sp3)
            orb = orb.combine_first(orb_add)
    orb = orb.EST

    if (type(rec_loc)==list) or (type(rec_loc)==_np.ndarray):
        add_all_angs(
            df=orb, 
            station_pos=rec_loc,
            nad=add_nad,
            el=add_el,
            az=add_az,
            dist=add_dist)
    return orb


def get_load_rnxsp3(start_date, end_date, station, directory, sp3pref='igs'):
    '''
    Start date format: "YYYY-MM-DD" - str
    End   date format: "YYYY-MM-DD" - str
    Frequency  format: "xU"        - str
    x -> number of units U
    U -> unit of time, e.g. "S" (sec), "H" (hour), "D" (day), etc.
    directory : path to download files to - str
    '''
    
    dt_list = _pd.date_range(
        start = start_date,
        end = end_date,
        freq = '1D'
    ).to_pydatetime()
    
    download_rinex3(dates=dt_list, station=station, dest=directory, dwn_src='cddis')
    download_sp3(dates=dt_list, dest=directory, pref=sp3pref, dwn_src='cddis')
    
    rnxs = list(_Path(directory).glob('*.rnx'))
    print('Loading rnx files ...')
    df_rnx = load_rnxs(rnxs)
    df_rnx = df_rnx.swaplevel(axis=1).EST

    sp3s = list(_Path(directory).glob('*.sp3'))
    rec_pos = _rnx_pos(rnxs[0])
    print('Loading sp3 files ...')
    df_sp3 = load_sp3s(sp3s, rec_loc=rec_pos)
    
    # Create long index for sp3 data (initially once every 15 min)
    dt_index = _pd.date_range(
        start = dt_list[0].strftime('%Y-%m-%d %H:%M:%S'),
        end = dt_list[0].strftime('%Y-%m-%d')+' 23:59:30',
        freq='30S'
    )

    for dt in dt_list[1:]:
        dt_index = dt_index.append(_pd.date_range(start=dt, end=dt.strftime('%Y-%m-%d')+' 23:59:30', freq='30S'))
    dt_count = len(dt_index)

    sv_index = []
    for val in df_sp3.index.get_level_values(1).unique().values:
        sv_index += [val]*dt_count

    dt_idx = list(dt_index.values)*len(df_sp3.index.get_level_values(1).unique().values)
    
    df_sp3_reset = df_sp3.reset_index()
    colsp3 = df_sp3_reset.columns
    df_sp3_reset.columns = ['time','sv']+list(colsp3[2:])
    df_sp3_reset['time'] = j20002datetime(df_sp3_reset['time'].values)
    df_sp3 = df_sp3_reset.set_index(['time','sv'])

    long_indices = _pd.MultiIndex.from_arrays([dt_idx,sv_index],names=['time','sv'])

    # Interpolate angle data in sp3 dataframe
    df_long_sp3 = _pd.merge(_pd.DataFrame(index=long_indices),df_sp3[['nad_ang','el_ang','az_ang','dist']],how='outer',on=['time','sv'])
    df_long_sp3['nad_ang'] = df_long_sp3.reset_index().pivot(index='time',columns='sv',values='nad_ang').interpolate(method='cubic').melt(ignore_index=False)[['value']].set_index(long_indices).value
    df_long_sp3['el_ang'] = df_long_sp3.reset_index().pivot(index='time',columns='sv',values='el_ang').interpolate(method='cubic').melt(ignore_index=False)[['value']].set_index(long_indices).value
    df_long_sp3['az_ang'] = df_long_sp3.reset_index().pivot(index='time',columns='sv',values='az_ang').interpolate(method='cubic').melt(ignore_index=False)[['value']].set_index(long_indices).value
    df_long_sp3['dist'] = df_long_sp3.reset_index().pivot(index='time',columns='sv',values='dist').interpolate(method='cubic').melt(ignore_index=False)[['value']].set_index(long_indices).value

    # Combine rnx and sp3 data - output dataframe
    df_rnx_reset = df_rnx.reset_index()
    df_rnx_reset['level_1'] = j20002datetime(df_rnx_reset['level_1'].values)

    cols = df_rnx_reset.columns
    df_rnx_reset = df_rnx_reset[cols[1:]]
    df_rnx_reset.columns = ['time','sv'] + list(cols[3:])

    return _pd.merge(df_rnx_reset.set_index(['time','sv']).sort_index(),df_long_sp3[['nad_ang','el_ang','az_ang','dist']],how='outer',on=['time','sv'])


def rmv_cons(num_list):
    '''
    Remove consecutive nums in list (there will be overlap in plotting the events anyway)
    '''
    num_list = sorted(num_list)
    if num_list:
        return [num_list[0]] + [
                                st 
                                for idx,st in enumerate(num_list) 
                                if (idx != 0) & (st > num_list[idx-1] + 10)
                                ]
    else:
        return []


def get_flex_sats(obs):
    '''
    Based on the given dataset 'obs', get list of Flex sats

    Input
    obs - 

    Output
    List of GPS satellite PRNs that are in site and have flex mode
    '''
    # List of GPS satellites found in the obs data store (Rinex)
    gps_list = [sat for sat in obs.sv.values if 'G' in sat]

    # Get list of GPS satellites that have flex power capabilities
    flex_blocks = ['GPS-IIR-M','GPS-IIF','GPS-IIIA']
    df_fb = df_sat_info(('Block',flex_blocks))
    flex_svns = list(df_fb['SVN'])

    # Establish start and end times for the period of interest
    st = obs.time.values[0]
    en = obs.time.values[-1]
    # Convert the SVNs to PRNs for the time period of interest
    flex_prns = []
    for svn in flex_svns:
        arr = svn_prn_dates(svn,st,en)['PRN'].values
        if arr.size == 0:
            continue
        else:
            flex_prns.append(arr[0])

    # Intersection of visible GPS sats and Flex enabled GPS sats:
    return [x for x in gps_list if x in flex_prns]



# Find possible flex power events
def find_flex_events(station, orb, obs, code, start_floor, end_floor, frac):

    # Get a list of the GPS satellites with flex mode and in view
    gps_flex = get_flex_sats(obs)

    # Predefine lists for memoing 
    st_idxs = []
    end_idxs = []
    
    # Set up the dataframe that will be exported:
    cols = ['Station','Time','Event Type','GPS Satellite']
    df = _pd.DataFrame(
        columns = cols
    )            
    
    for gps in gps_flex:
        
        # The values of the code being investigated
        vals = obs[code].sel(sv = gps).values
        
        # Look for flex events by comparing current value to four increments ago
        for i,v in enumerate(vals[4:]):
            
            # If current greater by some fraction 'frac', store in df as 'Start'
            if (v > vals[i]*(1.0+frac)) & (v > start_floor):
                #print(f'GPS Satellite {gps} has possible start event at index {i+4}')
                if i+4 not in st_idxs:
                    st_idxs.append(i+4)
                    new_row = {
                        'Station':station,
                        'Time':obs.time.values[i+4],
                        'Event Type':'Start',
                        'GPS Satellite':gps
                    }
                    df = df.append(new_row,ignore_index=True)
            
            # If current less by some fraction 'frac', store in df as 'End'
            elif (v < vals[i]*(1.0-frac)) & (v > end_floor):
                #print(f'GPS Satellite {gps} has possible end event at index {i+4}')
                if i+4 not in end_idxs:
                    end_idxs.append(i+4)
                    new_row = {
                        'Station':station,
                        'Time':obs.time.values[i+4],
                        'Event Type':'End',
                        'GPS Satellite':gps
                    }
                    df = df.append(new_row,ignore_index=True)

    sts = rmv_cons(st_idxs)
    ens = rmv_cons(end_idxs)

    return sts, ens, df


# Use a quick plot function (not using the obs_code_plot here)
def quick_plot(obs, code, station, st_idx, end_idx, title_add):
    

    gps_flex = get_flex_sats(obs)

    df = _pd.DataFrame(
        data = obs[code].sel(sv = gps_flex).values[st_idx:end_idx],
        index = obs.time.values[st_idx:end_idx],
        columns = gps_flex
    )

    vis_gps_list = [sat for sat in gps_flex if not df[sat].isnull().all()]

    fig1,ax1 = _plt.subplots() 
    df[vis_gps_list].plot(ax = ax1)
    ax1.legend(loc = 'upper left')
    ax1.set_ylabel('Signal strength for obs code: '+code+' (dB-Hz)')
    dts = _pd.to_datetime(obs.time.values[st_idx+30])
    
    

    if not _Path.exists(_Path('flex_plots')):
        _Path.mkdir(_Path('flex_plots')) 

    if not _Path.exists(_Path(f'flex_plots/{dts.strftime("%Y-%m-%d")}')):
        _Path.mkdir(_Path(f'flex_plots/{dts.strftime("%Y-%m-%d")}'))

    fname = f'{station}_'+dts.strftime('%Y-%m-%d_%H%M_')+f'{title_add}_{code}'
    fig1.suptitle(fname)
    fig1.savefig(f'flex_plots/{dts.strftime("%Y-%m-%d")}/{fname}.png',format='png')
    _plt.close()
    return #fig1.show()


def add_all_angs(
    df,
    station_pos,
    nad=True,
    el=True,
    az=True,
    dist=True,
    return_lists=False):
    '''
    Add new columns to dataframe (dist, nad_ang, el_ang and/or az_ang) after 
    calculating nadir, elevation and/or azimuth angles and distance between satellite
    and given site 'station_pos'
    Default is to add all options these options but can be set to False (nad, el, az, dist)
    The raw data can also be returned as a tuple of lists (nad_angs, el_angs, az_angs, distances)
    '''
    
    sat_pos_arr = df[['X','Y','Z']].to_numpy(dtype=float)
    disp_vec_arr = sat_pos_arr - (station_pos.reshape(1,3)/1000)
    
    nad_angs = []
    el_angs = []
    az_angs = []
    distances = []
    
    sat_rec_dist =_np.linalg.norm(disp_vec_arr,axis=1)
    sat_norm =_np.linalg.norm(sat_pos_arr,axis=1)

    if nad:
        df['nad_ang'] =_np.arccos(inner1d(sat_pos_arr, disp_vec_arr)/(sat_norm*sat_rec_dist))*(180/_np.pi)    
    
    if el or az:
        station_llh = xyz2llh_heik(station_pos.T)[0]
        R = llh2rot(_np.array([station_llh[0]]),_np.array([station_llh[1]]))

        enu_pos =_np.matmul(R[0],disp_vec_arr.T).T
        enu_bar = enu_pos /_np.linalg.norm(enu_pos,axis=1).reshape(len(enu_pos),1)
        
        if el:
            df['el_ang'] =_np.arcsin(enu_bar[:,2])*(180/_np.pi)
        if az:
            df['az_ang'] =_np.arctan2(enu_bar[:,0],enu_bar[:,1])
    
    if dist:
        df['dist'] = sat_rec_dist

    if return_lists:
        return nad_angs, el_angs, az_angs, distances



if __name__ == "__main__":

    # Introduce command line parser
    parser = argparse.ArgumentParser(
        description = 'Given a station and a time period, find possible flex power events'
        )
    
    # Command line function arguments
    parser.add_argument(
        "station", 
        help = "IGS GPS station name - must be new RINEX3 format - 9 characters"
        )        
    
    parser.add_argument(
        "st_date", 
        help = "Start Date in YYYY-MM-DD format"
        )
    
    parser.add_argument(
        "en_date", 
        help = "End Date in YYYY-MM-DD format"
        )

    parser.add_argument(
        "obs_codes",
        help = 'RINEX3 Observation code/s to search, e.g. "S1W" or "S2W" '
        )

    parser.add_argument(
        "start_floor",
        help = 'Min. Decibel-Hertz level at which to search for start of flex events (anything below ignored)'
        )

    parser.add_argument(
        "end_floor",
        help = 'Min. Decibel-Hertz level at which to search for end of flex events (anything below ignored)'
        )

    parser.add_argument(
        "frac",
        help = 'Fractional increase/decrease used to identify Start/End of event'
        )

    parser.add_argument(
        "el_min",
        help = 'Min. elevation angle of satellite to consider (anything below ignored)'
        )

    parser.add_argument(
        "dwn_dir",
        help = 'The download directory for RINEX3 and sp3 files'
        )


    parser.add_argument(
        "-p",
        "--plot",
        help = 'Produce plots of Flex events',
        action = "store_true"
    )


    # Get command line args:
    args = parser.parse_args()
    # And start assigning to variables:
    station = args.station
    st_date = _pd.to_datetime(args.st_date)
    en_date = _pd.to_datetime(args.en_date)
    codes = args.obs_codes
    st_lvl = float(args.start_floor)
    en_lvl = float(args.end_floor)
    frac = float(args.frac)
    el_min = float(args.el_min)
    dwn_dir = args.dwn_dir

    date_range = [st_date + _timedelta(days = x) for x in range((en_date - st_date).days + 1)]

    for date in date_range:
        # Get (download) or just load the necessary rinex and sp3 files for given day
        obs = get_load_rinex(station, date.strftime('%Y'), date.strftime('%j'), codes)
        sp3 = get_load_sp3(date.year, date.dayofyear)

        # Calculate the elevation angle for all satellites and add to obs xarrau DataSet
        obs = add_elevation_angles(sp3,obs)

        # Filter for values that have elevation angles greater than 'el_min'
        obs_el_mask = obs.where(obs.el_ang > el_min)

        # Run through each inputted code
        for code in codes.split(','): 
            
            

            sts, ens, df = find_flex_events(
                                            station = station, 
                                            orb = sp3, 
                                            obs = obs_el_mask, 
                                            code = code, 
                                            start_floor = st_lvl, 
                                            end_floor = en_lvl, 
                                            frac = frac
            )

            # If there are any events detected (non-empty df) write to a .csv file
            if not df.empty:
                # Order dataframe in terms of time:
                df.set_index('Time', inplace=True)
                df.sort_index(inplace=True)
                cl = sts + ens
                dts = _pd.to_datetime(obs.time.values[cl[0]])
                
                # Check directories exist:
                if not _Path.exists(_Path('flex_events')):
                    _Path.mkdir(_Path('flex_events'))
                if not _Path.exists(_Path(f'flex_events/{dts.strftime("%Y-%m-%d")}')):
                    _Path.mkdir(_Path(f'flex_events/{dts.strftime("%Y-%m-%d")}'))
                
                # Save dataframe to csv
                fname = f'{station}_'+dts.strftime('%Y-%m-%d_%H%M_')+f'_{code}'
                df.to_csv(f'flex_events/{dts.strftime("%Y-%m-%d")}/{fname}.csv')

                # If the plot option has been chosen in the 
                if args.plot:
                    
                    if sts:
                        for pt in sts:
                            # Start and end points/indexes to the plot
                            stpt = pt-30
                            enpt = pt+30
                            # Adjust if start points are before earliest or after latest index:
                            if stpt < 0:
                                stpt = 0
                            
                            if enpt > 2880:
                                enpt = 2880
                            # Plot
                            quick_plot(
                                obs_el_mask,
                                code,
                                station,
                                stpt,
                                enpt,
                                'start'
                            )

                    if ens:
                        for pt in ens:
                            stpt = pt-30
                            enpt = pt+30

                            if stpt < 0:
                                stpt = 0
                            
                            if enpt > 2880:
                                enpt = 2880

                            quick_plot(
                                obs_el_mask,
                                code,
                                station,
                                stpt,
                                enpt,
                                'end'
                            )




'''
For the new find_flex_events 
- X get sp3 and rinex 3 files 
- X load using new read_sp3 and read_rnx functions 
- X combine using code I've used in jupyter notebook
- add all angles and distances
- filter based on elevation
- search for flex events 
'''