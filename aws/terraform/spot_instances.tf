# AWS Spot Instance Configuration for Phase 1 SPSA
# 16-32 workers, 2-4 vCPUs each, capacity-optimized

resource "aws_launch_template" "prometheus_tuning" {
  name_prefix   = "prometheus-phase1-"
  image_id      = "ami-0c55b159cbfafe1f0"  # Ubuntu 22.04
  instance_type = "c6i.xlarge"  # 4 vCPUs, 8 GB RAM
  
  iam_instance_profile {
    name = "PrometheusS3Access"
  }
  
  block_device_mappings {
    device_name = "/dev/sda1"
    ebs {
      volume_size = 30
      volume_type = "gp3"
    }
  }
  
  user_data = base64encode(<<-EOF
    #!/bin/bash
    set -e
    
    # Setup script
    wget https://raw.githubusercontent.com/your-repo/aws/setup_instance.sh
    chmod +x setup_instance.sh
    ./setup_instance.sh
    
    # Start generation
    cd ~/prometheus-tuning
    screen -dmS games ./generate_games.sh 5000 games_$(hostname).pgn
  EOF
  )
  
  tag_specifications {
    resource_type = "instance"
    tags = {
      Name = "Prometheus-Phase1-Worker"
      Phase = "1"
      Purpose = "SPSA-Tuning"
    }
  }
}

resource "aws_spot_fleet_request" "prometheus_workers" {
  allocation_strategy      = "capacityOptimized"
  target_capacity          = 24  # 24 workers
  fleet_type               = "maintain"
  terminate_instances_with_expiration = true
  
  launch_template_config {
    launch_template_specification {
      id      = aws_launch_template.prometheus_tuning.id
      version = "$Latest"
    }
    
    overrides {
      instance_type     = "c6i.xlarge"
      spot_price        = "0.08"
      weighted_capacity = 1
      priority          = 1
    }
    
    overrides {
      instance_type     = "c5.xlarge"
      spot_price        = "0.08"
      weighted_capacity = 1
      priority          = 2
    }
    
    overrides {
      instance_type     = "t3.xlarge"
      spot_price        = "0.08"
      weighted_capacity = 1
      priority = 3
    }
  }
}

# Outputs
output "spot_fleet_id" {
  value = aws_spot_fleet_request.prometheus_workers.id
}
