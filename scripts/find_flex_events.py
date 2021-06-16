'''
NEEDS TO BE RE-WRITTEN TO USE NEW DIRECTORY STRUCTURE (pull functions from gn_io.gn_download)

Given a station and time range, find flex events.

With flex events it is important to remember only GPS Block IIR-M and onwards have this capability
Therefore, we need to choose GPS satellites that can actually have flex power events
'''
import sys
import argparse

import numpy as np
import pandas as pd

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import timedelta
from pathlib import Path

from get_sp3 import get_sp3, gpsweekD
from get_rinex3 import get_rinex
import geo_funcs as gf
from read_metadata import df_sat_info, svn_prn_dates


def get_load_rinex(station, year, doy, codes, dwndir):
    '''
    Get and/or load the Rinex3 file for a given day

    Input:
    station - IGS station of interest - str
    year - the year of interest - str
    doy - doy of year - str
    codes - the observation codes of interest - comma separated str
    '''

    rnx_filepath = Path(f"/home/ron-maj/pea/proc/data/{station}_R_{year}{doy}0000_01D_30S_MO.crx")
    if not rnx_filepath.is_file():
        get_rinex(year,doy,station,'/home/ron-maj/pea/proc/data')
    else:
        print('\ncrx file already exists')
        print(rnx_filepath)

    return gr.load(
        rnx_filepath,
        meas = codes.split(','),
        use = 'G'
        )


def get_load_sp3(year, doy):
    '''
    Get and/or load the sp3 file for a given day

    Input:
    year - the year of interest - str
    doy - doy of year - str

    Output
    Xarray.Dataset
    '''    

    # Filename we are looking for:
    gpswkD = gpsweekD(year,doy)
    filename = f'igs{gpswkD}.sp3'
    sp3_filepath = Path(f"/home/ron-maj/pea/proc/products/{filename}")
    
    if not sp3_filepath.is_file():
        get_sp3(int(year),int(doy))
        Path(f'sp3_files/{filename}').replace(sp3_filepath)
    
    else:
        print('\nsp3 file already exists')
        print(sp3_filepath)
    
    return gr.load(sp3_filepath)


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


def add_elevation_angles(orb, obs):

    # Run through all GPS satellites in the dataset and assign
    # an elevation angle to each data point
    el_data = []

    for gps in obs.sv.values:

        # Create a look up array of elevation angles
        el_angles = []
        orb_count = 0
        
        for time_val in obs.sel(sv = gps).time.values:
            
            # Convert to pandas timestamp:
            ts = pd.to_datetime(time_val)

            # Go through and create elevation angle list for each datapoint
            # Cover the edge case first
            if (ts.hour == 23) & (ts.minute == 45) & (ts.second == 0):
                try:
                    diff = el_angles[-1] - el_angles[-2]
                except IndexError:
                    #print(el_angles)
                    #print(time_val)
                    #print(orb.time.values[orb_count])
                    #print(obs.position)
                    #print(orb.sel(sv = gps).position.values[orb_count]*1000)
                    diff = 0.1
                
                i0 = gf.el_ang(obs.position,
                            orb.sel(sv = gps).position.values[orb_count]*1000)
                i0 = i0*(180/np.pi)
                
                i1 = i0 + 30*diff

                if i1 > 90:
                    i1 = 89
                elif i1 < -90:
                    i1 = -89

                el_angles += list(np.linspace(i0,i1,30))
            
            # In the general case, create a linear progression of elevation angles between
            # the 15 min time periods covered in the sp3 file each time one of the 
            # increments match up with those in the obs data.
            elif time_val == orb.time.values[orb_count]:
                
                i0 = gf.el_ang(obs.position,
                            orb.sel(sv = gps).position.values[orb_count]*1000)
                i0 = i0*(180/np.pi)

                i1 = gf.el_ang(obs.position,
                            orb.sel(sv = gps).position.values[orb_count+1]*1000)
                i1 = i1*(180/np.pi)

                el_angles += list(np.linspace(i0,i1,31)[:-1])
                orb_count += 1

            # If the time periods do not match up, continue on
            else:
                continue
        
        # Create dataframe of elevation angle data for given gps satellite
        df = pd.DataFrame(
            data = el_angles[:len(obs.time)],
            index = obs.sel(sv = gps).time.values,
            columns = [gps]
            )
        # Append this dataframe to the list of all elevation angle data from previous gps satellites
        el_data.append(df)
    
    # Concat dataframes of each individual satellite to one master dataframe:
    dat = pd.concat(el_data,1)
    # Add master dataframe of elevation angles to the xr Dataset
    xDA = xr.DataArray(dat.values, dims = ['time','sv'], coords = {'time':dat.index,'sv':dat.columns})
    obs['el_ang'] = xDA
    '''
    try:
        el_DA = xr.DataArray(
            dat,
            dims = {'sv','time'},
            coords = {'sv': obs.sv, 'time': obs.time}
        )
        obs['el_ang'] = el_DA
    except ValueError:
        el_DA1 = xr.DataArray(
            dat,
            dims = {'time','sv'},
            coords = {'time': obs.time, 'sv': obs.sv}
        )
        obs['el_ang'] = el_DA1
    '''    
    return obs



# Find possible flex power events
def find_flex_events(station, orb, obs, code, start_floor, end_floor, frac):

    # Get a list of the GPS satellites with flex mode and in view
    gps_flex = get_flex_sats(obs)

    # Predefine lists for memoing 
    st_idxs = []
    end_idxs = []
    
    # Set up the dataframe that will be exported:
    cols = ['Station','Time','Event Type','GPS Satellite']
    df = pd.DataFrame(
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

    df = pd.DataFrame(
        data = obs[code].sel(sv = gps_flex).values[st_idx:end_idx],
        index = obs.time.values[st_idx:end_idx],
        columns = gps_flex
    )

    vis_gps_list = [sat for sat in gps_flex if not df[sat].isnull().all()]

    fig1,ax1 = plt.subplots() 
    df[vis_gps_list].plot(ax = ax1)
    ax1.legend(loc = 'upper left')
    ax1.set_ylabel('Signal strength for obs code: '+code+' (dB-Hz)')
    dts = pd.to_datetime(obs.time.values[st_idx+30])
    
    

    if not Path.exists(Path('flex_plots')):
        Path.mkdir(Path('flex_plots')) 

    if not Path.exists(Path(f'flex_plots/{dts.strftime("%Y-%m-%d")}')):
        Path.mkdir(Path(f'flex_plots/{dts.strftime("%Y-%m-%d")}'))

    fname = f'{station}_'+dts.strftime('%Y-%m-%d_%H%M_')+f'{title_add}_{code}'
    fig1.suptitle(fname)
    fig1.savefig(f'flex_plots/{dts.strftime("%Y-%m-%d")}/{fname}.png',format='png')
    plt.close()
    return #fig1.show()


def add_all_angs(
    df_sats_pos,
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
    
    sat_pos_arr = df_sats_pos[['x','y','z']].to_numpy(dtype=float)
    disp_vec_arr = sat_pos_arr - (station_pos.reshape(1,3)/1000)
    
    nad_angs = []
    el_angs = []
    az_angs = []
    distances = []
    
    sat_rec_dist =_np.linalg.norm(disp_vec_arr,axis=1)
    sat_norm =_np.linalg.norm(sat_pos_arr,axis=1)

    if nad:
        df_sats_pos['nad_ang'] =_np.arccos(inner1d(sat_pos_arr, disp_vec_arr)/(sat_norm*sat_rec_dist))*(180/_np.pi)    
    
    if el or az:
        station_llh = xyz2llh_heik(station_pos.T)[0]
        R = llh2rot(_np.array([station_llh[0]]),_np.array([station_llh[1]]))

        enu_pos =_np.matmul(R[0],disp_vec_arr.T).T
        enu_bar = enu_pos /_np.linalg.norm(enu_pos,axis=1).reshape(len(enu_pos),1)
        
        if el:
            df_sats_pos['el_ang'] =_np.arcsin(enu_bar[:,2])*(180/_np.pi)
        if az:
            df_sats_pos['az_ang'] =_np.arctan2(enu_bar[:,0],enu_bar[:,1])
    
    if dist:
        df_sats_pos['dist'] = sat_rec_dist

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
        "-p",
        "--plot",
        help = 'Produce plots of Flex events',
        action = "store_true"
    )


    # Get command line args:
    args = parser.parse_args()
    # And start assigning to variables:
    station = args.station
    st_date = pd.to_datetime(args.st_date)
    en_date = pd.to_datetime(args.en_date)
    codes = args.obs_codes
    st_lvl = float(args.start_floor)
    en_lvl = float(args.end_floor)
    frac = float(args.frac)
    el_min = float(args.el_min)

    date_range = [st_date + timedelta(days = x) for x in range((en_date - st_date).days + 1)]

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
                dts = pd.to_datetime(obs.time.values[cl[0]])
                
                # Check directories exist:
                if not Path.exists(Path('flex_events')):
                    Path.mkdir(Path('flex_events'))
                if not Path.exists(Path(f'flex_events/{dts.strftime("%Y-%m-%d")}')):
                    Path.mkdir(Path(f'flex_events/{dts.strftime("%Y-%m-%d")}'))
                
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