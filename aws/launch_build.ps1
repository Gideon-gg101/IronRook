
# AWS Build Launcher

$RoleName = "PrometheusEC2Role"
$PolicyArn = "arn:aws:iam::aws:policy/AmazonS3FullAccess"
$ProfileName = "PrometheusS3Access"

# 1. Create Role
Write-Host "Creating IAM Role..."
aws iam create-role --role-name $RoleName --assume-role-policy-document file://aws/trust_policy.json


# 2. Attach Policy
Write-Host "Attaching Policy..."
aws iam attach-role-policy --role-name $RoleName --policy-arn $PolicyArn

# 3. Create Instance Profile
Write-Host "Creating Instance Profile..."
aws iam create-instance-profile --instance-profile-name $ProfileName 2>$null

# 4. Add Role to Profile
Write-Host "Adding Role to Profile..."
aws iam add-role-to-instance-profile --instance-profile-name $ProfileName --role-name $RoleName 2>$null

# Wait for eventual consistency
Write-Host "Waiting 10s for IAM propagation..."
Start-Sleep -Seconds 10

# 5. Launch Instance
Write-Host "Launching Build Instance..."
$UserData = "#!/bin/bash
yum install -y gcc-c++ cmake make aws-cli git unzip
aws s3 cp s3://prometheus-tuning-data/prometheus_source.zip /tmp/source.zip
cd /tmp
unzip source.zip
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=""-march=x86-64 -mtune=generic""
make -j2
aws s3 cp IronRook s3://prometheus-tuning-data/engine/IronRook_linux
shutdown -h now
"

$EncodedUserData = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($UserData))

$InstanceId = aws ec2 run-instances `
    --image-id ami-0532be01f26a3de55 `
    --instance-type t3.micro `
    --count 1 `
    --iam-instance-profile Name=$ProfileName `
    --user-data $EncodedUserData `
    --tag-specifications 'ResourceType=instance,Tags=[{Key=Name,Value=Prometheus-Builder}]' `
    --query 'Instances[0].InstanceId' `
    --output text

Write-Host "Launched Instance: $InstanceId"
Write-Host "Build started. Instance will shutdown automatically."
Write-Host "Monitor with: aws s3 ls s3://prometheus-tuning-data/engine/"
