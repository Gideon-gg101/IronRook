import boto3
import json
import time

def setup_iam():
    iam = boto3.client('iam')
    
    # 1. Create Role
    role_name = 'PrometheusEC2Role'
    try:
        assume_role_policy = {
            "Version": "2012-10-17",
            "Statement": [{
                "Effect": "Allow",
                "Principal": {"Service": "ec2.amazonaws.com"},
                "Action": "sts:AssumeRole"
            }]
        }
        iam.create_role(
            RoleName=role_name,
            AssumeRolePolicyDocument=json.dumps(assume_role_policy)
        )
        print(f"Created role: {role_name}")
    except iam.exceptions.EntityAlreadyExistsException:
        print(f"Role {role_name} already exists")

    # 2. Attach S3 Full Access
    try:
        iam.attach_role_policy(
            RoleName=role_name,
            PolicyArn='arn:aws:iam::aws:policy/AmazonS3FullAccess'
        )
        print("Attached S3FullAccess policy")
    except Exception as e:
        print(f"Error attaching policy: {e}")

    # 3. Create Instance Profile
    profile_name = 'PrometheusS3Access'
    try:
        iam.create_instance_profile(InstanceProfileName=profile_name)
        print(f"Created instance profile: {profile_name}")
    except iam.exceptions.EntityAlreadyExistsException:
        print(f"Instance profile {profile_name} already exists")

    # 4. Add Role to Profile
    try:
        iam.add_role_to_instance_profile(
            InstanceProfileName=profile_name,
            RoleName=role_name
        )
        print("Added role to instance profile")
    except iam.exceptions.LimitExceededException:
        print("Role already added to profile (limit exceeded usually means duplicate)")
    except iam.exceptions.EntityAlreadyExistsException:
        print("Role already added to profile")
    except Exception as e:
        if "Cannot exceed quota" in str(e):
             print("Role likely already attached")
        else:
             print(f"Error adding role to profile: {e}")

    # Wait for propagation
    print("Waiting 10s for IAM propagation...")
    time.sleep(10)

def launch_build_instance():
    ec2 = boto3.client('ec2', region_name='us-east-1')
    
    # UserData script
    user_data = """#!/bin/bash
yum install -y gcc-c++ cmake make aws-cli git unzip
aws s3 cp s3://prometheus-tuning-data/prometheus_source.zip /tmp/source.zip
cd /tmp
unzip source.zip
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=x86-64 -mtune=generic"
make -j2
aws s3 cp IronRook s3://prometheus-tuning-data/engine/IronRook_linux
shutdown -h now
"""
    
    try:
        print("Launching build instance...")
        response = ec2.run_instances(
            ImageId='ami-026992d753d5622bc', # Amazon Linux 2 in us-east-1
            InstanceType='t3.micro',
            MinCount=1,
            MaxCount=1,
            IamInstanceProfile={'Name': 'PrometheusS3Access'},
            UserData=user_data,
            TagSpecifications=[{
                'ResourceType': 'instance',
                'Tags': [{'Key': 'Name', 'Value': 'Prometheus-Builder'}]
            }]
        )
        instance_id = response['Instances'][0]['InstanceId']
        print(f"Launched instance: {instance_id}")
        return instance_id
    except Exception as e:
        print(f"Error launching instance: {e}")
        return None

if __name__ == '__main__':
    setup_iam()
    launch_build_instance()
