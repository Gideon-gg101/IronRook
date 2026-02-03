
# Deploy Fleet to AWS
# 1. Update worker script in launch template
# 2. Launch Spot Fleet

# Encode worker script
$WorkerScript = Get-Content -Raw aws/worker.sh
$WorkerScriptB64 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($WorkerScript))

# Update Launch Template JSON
$Template = Get-Content aws/launch_template.json | ConvertFrom-Json
$Template.LaunchTemplateData.UserData = $WorkerScriptB64
$Template | ConvertTo-Json -Depth 10 | Set-Content aws/launch_template_final.json

# Create/Update Launch Template
Write-Host "Updating Launch Template with new version..."
$LaunchTemplateExists = aws ec2 describe-launch-templates --launch-template-names prometheus-phase1-worker 2>$null
if ($null -eq $LaunchTemplateExists) {
    Write-Host "Creating new Launch Template..."
    aws ec2 create-launch-template --launch-template-name prometheus-phase1-worker --launch-template-data (Get-Content aws/launch_template_final.json -Raw | ConvertFrom-Json | Select-Object -ExpandProperty LaunchTemplateData | ConvertTo-Json -Depth 10)
}
else {
    Write-Host "Creating new version for existing Launch Template..."
    aws ec2 create-launch-template-version --launch-template-name prometheus-phase1-worker --launch-template-data (Get-Content aws/launch_template_final.json -Raw | ConvertFrom-Json | Select-Object -ExpandProperty LaunchTemplateData | ConvertTo-Json -Depth 10)
}

# Get Template ID
$TemplateId = aws ec2 describe-launch-templates --launch-template-names prometheus-phase1-worker --query 'LaunchTemplates[0].LaunchTemplateId' --output text
Write-Host "Template ID: $TemplateId"

# Update Spot Fleet Config
$SpotConfig = Get-Content aws/spot_fleet_config.json | ConvertFrom-Json
$SpotConfig.LaunchTemplateConfigs[0].LaunchTemplateSpecification.LaunchTemplateId = $TemplateId
$SpotConfig | ConvertTo-Json -Depth 10 | Set-Content aws/spot_fleet_config_final.json

# Launch Fleet
Write-Host "Launching Spot Fleet..."
$FleetId = aws ec2 request-spot-fleet --spot-fleet-request-config file://aws/spot_fleet_config_final.json --query 'SpotFleetRequestId' --output text

Write-Host "Fleet Launched: $FleetId"
$FleetId | Out-File fleet_id.txt
Write-Host "Monitor with: aws ec2 describe-spot-fleet-instances --spot-fleet-request-id $FleetId"
