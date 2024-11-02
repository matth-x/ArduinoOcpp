import os
import sys
import requests
import paramiko
import base64
import traceback
import io
import json
import time
import pandas as pd


requests.packages.urllib3.disable_warnings() # avoid the URL to be printed to console

# Test case selection (commented out a few to simplify testing for now)
testcase_name_list = [
    'TC_B_06_CS',
    #'TC_B_07_CS',
    #'TC_B_09_CS',
    'TC_B_10_CS',
    #'TC_B_11_CS',
    #'TC_B_12_CS',
    'TC_B_13_CS',
    #'TC_B_32_CS',
    #'TC_B_34_CS',
    'TC_B_35_CS',
    #'TC_B_36_CS',
    #'TC_B_37_CS',
    'TC_B_39_CS',
    #'TC_C_02_CS',
    #'TC_C_04_CS',
    'TC_C_06_CS',
    #'TC_C_07_CS',
    #'TC_C_49_CS',
    'TC_E_01_CS',
    #'TC_E_02_CS',
    #'TC_E_03_CS',
    'TC_E_04_CS',
    #'TC_E_05_CS',
    #'TC_E_06_CS',
    'TC_E_07_CS',
    #'TC_E_09_CS',
    #'TC_E_13_CS',
    'TC_E_15_CS',
    #'TC_E_17_CS',
    #'TC_E_20_CS',
    'TC_E_21_CS',
    #'TC_E_24_CS',
    #'TC_E_25_CS',
    'TC_E_28_CS',
    #'TC_E_29_CS',
    #'TC_E_30_CS',
    'TC_E_31_CS',
    #'TC_E_32_CS',
    #'TC_E_33_CS',
    'TC_E_34_CS',
    #'TC_E_35_CS',
    #'TC_E_39_CS',
    'TC_F_01_CS',
    #'TC_F_02_CS',
    #'TC_F_03_CS',
    'TC_F_04_CS',
    #'TC_F_05_CS',
    #'TC_F_06_CS',
    'TC_F_07_CS',
    #'TC_F_08_CS',
    #'TC_F_09_CS',
    'TC_F_10_CS',
    #'TC_F_11_CS',
    #'TC_F_12_CS',
    'TC_F_13_CS',
    #'TC_F_14_CS',
    #'TC_F_20_CS',
    'TC_F_23_CS',
    #'TC_F_24_CS',
    #'TC_F_26_CS',
    'TC_F_27_CS',
    #'TC_G_01_CS',
    #'TC_G_02_CS',
    'TC_G_03_CS',
    #'TC_G_04_CS',
    #'TC_G_05_CS',
    'TC_G_06_CS',
    #'TC_G_07_CS',
    #'TC_G_08_CS',
    'TC_G_09_CS',
    #'TC_G_10_CS',
    #'TC_G_11_CS',
    'TC_G_12_CS',
    #'TC_G_13_CS',
    #'TC_G_14_CS',
    'TC_G_15_CS',
    #'TC_G_16_CS',
    #'TC_G_17_CS',
    'TC_J_07_CS',
    #'TC_J_08_CS',
    #'TC_J_09_CS',
    'TC_J_10_CS',
]

# Result data set
df = pd.DataFrame(columns=['FN_BLOCK', 'Testcase', 'Pass', 'Heap usage (Bytes)'])
df.index.names = ['TC_ID']

max_memory_total = 0
min_memory_base = 1000 * 1000 * 1000

def connect_ssh():

    if not os.path.isfile(os.path.join('tests', 'benchmarks', 'scripts', 'id_ed25519')):
        file = open(os.path.join('tests', 'benchmarks', 'scripts', 'id_ed25519'), 'w')
        file.write(os.environ['SSH_LOCAL_PRIV'])
        file.close()
        print('SSH ID written to file')

    client = paramiko.SSHClient()
    client.get_host_keys().add('cicd.micro-ocpp.com', 'ssh-ed25519', paramiko.pkey.PKey.from_type_string('ssh-ed25519', base64.b64decode(os.environ['SSH_HOST_PUB'])))
    client.connect('cicd.micro-ocpp.com', username='ocpp', key_filename=os.path.join('tests', 'benchmarks', 'scripts', 'id_ed25519'), look_for_keys=False)
    return client

def close_ssh(client: paramiko.SSHClient):

    client.close()

def deploy_simulator():

    print('Deploy Simulator')

    client = connect_ssh()

    print('   - stop Simulator, if still running')
    stdin, stdout, stderr = client.exec_command('killall -s SIGINT mo_simulator')

    print('   - clean previous deployment')
    stdin, stdout, stderr = client.exec_command('rm -rf ' + os.path.join('MicroOcppSimulator'))

    print('   - init folder structure')
    sftp = client.open_sftp()
    sftp.mkdir(os.path.join('MicroOcppSimulator'))
    sftp.mkdir(os.path.join('MicroOcppSimulator', 'build'))
    sftp.mkdir(os.path.join('MicroOcppSimulator', 'public'))
    sftp.mkdir(os.path.join('MicroOcppSimulator', 'mo_store'))

    print('   - upload files')
    sftp.put(  os.path.join('MicroOcppSimulator', 'build', 'mo_simulator'),
               os.path.join('MicroOcppSimulator', 'build', 'mo_simulator'))
    sftp.chmod(os.path.join('MicroOcppSimulator', 'build', 'mo_simulator'), 0O777)
    sftp.put(  os.path.join('MicroOcppSimulator', 'public', 'bundle.html.gz'),
               os.path.join('MicroOcppSimulator', 'public', 'bundle.html.gz'))
    sftp.close()
    close_ssh(client)
    print('   - done')

def cleanup_simulator():

    print('Clean up Simulator')

    client = connect_ssh()

    print('   - stop Simulator, if still running')
    stdin, stdout, stderr = client.exec_command('killall -s SIGINT mo_simulator')

    print('   - clean deployment')
    stdin, stdout, stderr = client.exec_command('rm -rf ' + os.path.join('MicroOcppSimulator'))

    close_ssh(client)
    print('   - done')

def setup_simulator():

    print('Setup Simulator')

    client = connect_ssh()

    print('   - stop Simulator, if still running')
    stdin, stdout, stderr = client.exec_command('killall -s SIGINT mo_simulator')

    print('   - clean state')
    stdin, stdout, stderr = client.exec_command('rm -rf ' + os.path.join('MicroOcppSimulator', 'mo_store', '*'))

    print('   - upload credentials')
    sftp = client.open_sftp()
    sftp.putfo(io.StringIO(os.environ['MO_SIM_CONFIG']),     os.path.join('MicroOcppSimulator', 'mo_store', 'simulator.jsn'))
    sftp.putfo(io.StringIO(os.environ['MO_SIM_OCPP_SERVER']),os.path.join('MicroOcppSimulator', 'mo_store', 'ws-conn-v201.jsn'))
    sftp.putfo(io.StringIO(os.environ['MO_SIM_API_CERT']),   os.path.join('MicroOcppSimulator', 'mo_store', 'api_cert.pem'))
    sftp.putfo(io.StringIO(os.environ['MO_SIM_API_KEY']),    os.path.join('MicroOcppSimulator', 'mo_store', 'api_key.pem'))
    sftp.putfo(io.StringIO(os.environ['MO_SIM_API_CONFIG']), os.path.join('MicroOcppSimulator', 'mo_store', 'api.jsn'))
    sftp.close()

    print('   - start Simulator')

    stdin, stdout, stderr = client.exec_command('mkdir -p logs && cd ' + os.path.join('MicroOcppSimulator') + ' && ./build/mo_simulator > ~/logs/sim_"$(date +%Y-%m-%d_%H-%M-%S.log)"')
    close_ssh(client)

    print('   - done')

def run_measurements():
    
    global max_memory_total
    global min_memory_base
    
    print("Fetch TCs from Test Driver")

    response = requests.get(os.environ['TEST_DRIVER_URL'] + '/ocpp2.0.1/CS/testcases/' + os.environ['TEST_DRIVER_CONFIG'], 
                            headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']},
                            verify=False)

    #print(json.dumps(response.json(), indent=4))

    testcases = []

    for i in response.json()['data']['testcasesData']:
        for j in i['data']:
            is_core = False
            for k in j['certification_profiles']:
                if k == 'Core':
                    is_core = True
                    break

            select = False
            for k in testcase_name_list:
                if j['testcase_name'] == k:
                    select = True
                    break

            if select:
                print(i['header'] + ' --- ' + j['functional_block'] + ' --- ' + j['description'])
                testcases.append(j)

    deploy_simulator()

    print('Get Simulator base memory data')
    setup_simulator()

    response = requests.post('https://cicd.micro-ocpp.com:8443/api/memory/reset', 
                             auth=(json.loads(os.environ['MO_SIM_API_CONFIG'])['user'],
                                   json.loads(os.environ['MO_SIM_API_CONFIG'])['pass']))
    print(f'Simulator API /memory/reset:\n > {response.status_code}')

    response = requests.get('https://cicd.micro-ocpp.com:8443/api/memory/info', 
                             auth=(json.loads(os.environ['MO_SIM_API_CONFIG'])['user'],
                                   json.loads(os.environ['MO_SIM_API_CONFIG'])['pass']))
    print(f'Simulator API /memory/info:\n > {response.status_code}, current heap={response.json()["total_current"]}, max heap={response.json()["total_max"]}')
    base_memory_level = response.json()["total_max"]
    min_memory_base = min(min_memory_base, response.json()["total_max"])

    print("Start Test Driver")

    response = requests.post(os.environ['TEST_DRIVER_URL'] + '/ocpp2.0.1/CS/session/start/' + os.environ['TEST_DRIVER_CONFIG'], 
                             headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']},
                             verify=False)
    print(f'Test Driver /*/*/session/start/*:\n > {response.status_code}')
    #print(json.dumps(response.json(), indent=4))

    for testcase in testcases:
        print('\nRun ' + testcase['functional_block'] + ' > ' + testcase['description'] + ' (' + testcase['testcase_name'] + ')')

        if testcase['testcase_name'] in df.index:
            print('Test case already executed - skip')
            continue

        setup_simulator()
        time.sleep(1)

        # Check connection
        simulator_connected = False
        for i in range(5):
            response = requests.get(os.environ['TEST_DRIVER_URL'] + '/sut_connection_status', 
                                    headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']},
                                    verify=False)
            print(f'Test Driver /sut_connection_status:\n > {response.status_code}')
            #print(json.dumps(response.json(), indent=4))
            if response.status_code == 200:
                simulator_connected = True
                break
            else:
                print(f'Waiting for the Simulator to connect ({i}) ...')
                time.sleep(3)
        
        if not simulator_connected:
            print('Simulator could not connect to Test Driver')
            raise Exception()
        
        response = requests.post('https://cicd.micro-ocpp.com:8443/api/memory/reset', 
                             auth=(json.loads(os.environ['MO_SIM_API_CONFIG'])['user'],
                                   json.loads(os.environ['MO_SIM_API_CONFIG'])['pass']))
        print(f'Simulator API /memory/reset:\n > {response.status_code}')
        
        test_response = requests.post(os.environ['TEST_DRIVER_URL'] + '/testcases/' + testcase['testcase_name'] + '/execute', 
                                 headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']},
                                 verify=False)
        print(f'Test Driver /testcases/{testcase["testcase_name"]}/execute:\n > {test_response.status_code}')
        #try:
        #    print(json.dumps(test_response.json(), indent=4))
        #except:
        #    print(' > No JSON')

        sim_response = requests.get('https://cicd.micro-ocpp.com:8443/api/memory/info', 
                             auth=(json.loads(os.environ['MO_SIM_API_CONFIG'])['user'],
                                   json.loads(os.environ['MO_SIM_API_CONFIG'])['pass']))
        print(f'Simulator API /memory/info:\n > {sim_response.status_code}, current heap={sim_response.json()["total_current"]}, max heap={sim_response.json()["total_max"]}')

        df.loc[testcase['testcase_name']] = [testcase['functional_block'], testcase['description'], 'x' if test_response.status_code == 200 and test_response.json()['data'][0]['verdict'] == "pass" else '-', str(sim_response.json()["total_max"] - min(base_memory_level, sim_response.json()["total_current"]))]

        max_memory_total = max(max_memory_total, sim_response.json()["total_max"])
        min_memory_base = min(min_memory_base, sim_response.json()["total_current"])

    print("Stop Test Driver")
    
    response = requests.post(os.environ['TEST_DRIVER_URL'] + '/session/stop', 
                             headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']},
                             verify=False)
    print(f'Test Driver /session/stop:\n > {response.status_code}')
    #print(json.dumps(response.json(), indent=4))

    cleanup_simulator()

    print('Store test results')

    # Add some meta information
    max_memory = 0
    for index, row in df.iterrows():
        memory = row['Heap usage (Bytes)']
        if memory.isdigit():
            max_memory = max(max_memory, int(memory))

    functional_blocks = set()
    for index, row in df.iterrows():
        functional_blocks.add(row['FN_BLOCK'])

    print(functional_blocks)

    for i in functional_blocks:
        df.loc[f'TC_{i[0]}'] = [i, f'**{i}**', ' ', ' ']

    df.loc['|MO_SIM_000'] = ['-', '**Simulator stats**', ' ', ' ']
    df.loc['|MO_SIM_010'] = ['-', 'Base memory occupation', ' ', str(min_memory_base)]
    df.loc['|MO_SIM_020'] = ['-', 'Test case maximum', ' ', str(max_memory)]
    df.loc['|MO_SIM_030'] = ['-', 'Total memory maximum', ' ', str(max_memory_total)]

    df.sort_index(inplace=True)
    
    print(df)

    df.to_csv('docs/assets/tables/heap_v201.csv',index=False,columns=['Testcase','Pass','Heap usage (Bytes)'])

    print('Stored test results to CSV')

def run_measurements_and_retry():

    if (    'TEST_DRIVER_URL'    not in os.environ or
            'TEST_DRIVER_CONFIG' not in os.environ or
            'TEST_DRIVER_KEY'    not in os.environ or
            'MO_SIM_CONFIG'      not in os.environ or
            'MO_SIM_OCPP_SERVER' not in os.environ or
            'MO_SIM_API_CERT'    not in os.environ or
            'MO_SIM_API_KEY'     not in os.environ or
            'MO_SIM_API_CONFIG'  not in os.environ or
            'SSH_LOCAL_PRIV'     not in os.environ or
            'SSH_HOST_PUB'       not in os.environ):
        sys.exit('\nCould not read environment variables')

    n_tries = 3

    for i in range(n_tries):

        try:
            run_measurements()
            print('\n **Test cases executed successfully**')
            break
        except:
            print(f'Error detected ({i+1})')

            traceback.print_exc()

            print("Stop Test Driver")    
            response = requests.post(os.environ['TEST_DRIVER_URL'] + '/session/stop', 
                                    headers={'Authorization': 'Bearer ' + os.environ['TEST_DRIVER_KEY']},
                                    verify=False)
            print(f'Test Driver /session/stop:\n > {response.status_code}')
            #print(json.dumps(response.json(), indent=4))

            cleanup_simulator()

            if i + 1 < n_tries:
                print('Retry test cases')
            else:
                print('\n **Test case execution aborted**')
                sys.exit('\nError running test cases')

run_measurements_and_retry()
