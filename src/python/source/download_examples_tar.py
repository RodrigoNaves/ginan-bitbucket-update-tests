import os
import tarfile
import urllib.request
import shutil
import base64
import hashlib

def get_checksum_S3(path2file):
    print('---computing md5 checksum---')
    with open(path2file,'rb') as file:
        filehash = hashlib.md5()
        while True:
            data = file.read(8 * 1024 * 1024)
            if len(data) == 0:
                break
            filehash.update(data)
    return base64.b64encode(filehash.digest())

def untar(file):
    with tarfile.open(file,"r:gz") as tar:
        destpath = os.path.dirname(file)
        print('Extracting {} to {}'.format(file,destpath))
        tar.extractall(path=destpath)
        
def download_examples_tar(url,relpath = '../../'):
    '''relpath configures output path relative to the sctipt location'''
    destfile = os.path.basename(url) #proc.tar.gz
    script_path = os.path.dirname(os.path.realpath(__file__))
    destfile = os.path.abspath(os.path.join(script_path,relpath,destfile))
    if os.path.exists(destfile):
        print('file found')
        with urllib.request.urlopen(url) as response:
            if response.status == 200:
                if get_checksum_S3(destfile) == (response.getheader('x-amz-meta-md5checksum')).encode():
                    print("MD5 verified, skipping the download step")
                    untar(destfile)
                else:
                    print("MD5 check failed, redownloading the file")
                    if os.path.exists(destfile): os.remove(destfile)

    if not os.path.exists(destfile):
        with urllib.request.urlopen(url) as response:
            if response.status == 200:
                print('Downloading from {} to {}'.format(url, destfile))
                with open(destfile, 'wb') as out_file: shutil.copyfileobj(response, out_file)
        untar(destfile)

url='https://peanpod.s3-ap-southeast-2.amazonaws.com/pea/proc.tar.gz'
download_examples_tar(url=url)