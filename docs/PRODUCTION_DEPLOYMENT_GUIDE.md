# PRODUCTION DEPLOYMENT GUIDE

## Pre-Deployment Checklist

### Code Validation
- [ ] All tests passing (`ctest --output-on-failure`)
- [ ] No compiler warnings
- [ ] No static analysis issues
- [ ] Code review completed
- [ ] Security audit passed

### Configuration
- [ ] Production config validated
- [ ] All environment variables set
- [ ] Database connections tested
- [ ] API keys configured
- [ ] Certificate paths verified

### Infrastructure
- [ ] Hardware meets minimum specs
- [ ] Network connectivity verified
- [ ] Storage space adequate (min 10GB)
- [ ] Memory sufficient (min 4GB)
- [ ] CPU performance verified

### Backup & Recovery
- [ ] Backup plan documented
- [ ] Recovery procedure tested
- [ ] Disaster recovery plan ready
- [ ] Data archival strategy ready
- [ ] Rollback procedure documented

### Monitoring & Logging
- [ ] Log rotation configured
- [ ] Monitoring agents installed
- [ ] Alert thresholds set
- [ ] Dashboard created
- [ ] On-call schedule ready

---

## Deployment Procedure

### Phase 1: Pre-Deployment (24 hours before)

```bash
# 1. Build release binary
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWITH_SIMD=ON
cmake --build . --config Release --parallel 4

# 2. Run full test suite
ctest --output-on-failure -V

# 3. Generate reports
ctest -D Continuous

# 4. Verify binary
file BulletHoleDetection
ldd BulletHoleDetection  # Check dependencies
```

### Phase 2: Staging Deployment (4 hours before)

```bash
# 1. Deploy to staging environment
scp -r build/BulletHoleDetection staging:/opt/bullet_detection/

# 2. Deploy configuration
scp config/production.json staging:/etc/bullet_detection/

# 3. Deploy start/stop scripts
scp scripts/start.sh staging:/opt/bullet_detection/
scp scripts/stop.sh staging:/opt/bullet_detection/
chmod +x staging:/opt/bullet_detection/*.sh

# 4. Run smoke tests
ssh staging "cd /opt/bullet_detection && ./test_deployment.sh"

# 5. Monitor for 1 hour
ssh staging "tail -f logs/bullet_detection.log"
```

### Phase 3: Production Deployment

```bash
# 1. Create backup of current version
ssh production "cp -r /opt/bullet_detection /opt/bullet_detection.backup.$(date +%Y%m%d)"

# 2. Deploy new version
scp -r build/BulletHoleDetection production:/opt/bullet_detection/

# 3. Deploy configuration
scp config/production.json production:/etc/bullet_detection/

# 4. Stop old service gracefully
ssh production "systemctl stop bullet_detection || /opt/bullet_detection/stop.sh"

# 5. Start new service
ssh production "systemctl start bullet_detection || /opt/bullet_detection/start.sh"

# 6. Verify service is running
ssh production "systemctl status bullet_detection"
ssh production "curl http://localhost:8080/health || ./check_health.sh"

# 7. Monitor initial startup (30 minutes)
ssh production "tail -f logs/bullet_detection.log | grep -E 'ERROR|CRITICAL'"
```

### Phase 4: Post-Deployment Validation

```bash
# 1. Check system health
./check_health.sh production

# 2. Run integration tests
./run_integration_tests.sh production

# 3. Verify performance
./benchmark_system.sh production

# 4. Check resource usage
ssh production "top -bn1 | head -20"
ssh production "df -h"
```

---

## Rollback Procedure

If issues occur, rollback immediately:

```bash
# 1. Stop new version
ssh production "systemctl stop bullet_detection"

# 2. Restore previous version
ssh production "rm -rf /opt/bullet_detection"
ssh production "mv /opt/bullet_detection.backup.$(date +%Y%m%d) /opt/bullet_detection"

# 3. Restart old version
ssh production "systemctl start bullet_detection"

# 4. Verify operation
ssh production "systemctl status bullet_detection"
ssh production "curl http://localhost:8080/health"

# 5. Investigate issues
ssh production "tail -100 logs/bullet_detection.log > /tmp/error_report.log"
scp production:/tmp/error_report.log ./investigation/
```

---

## Health Check

```bash
#!/bin/bash

HOST=${1:-localhost}
PORT=8080

echo "Checking system health on $HOST:$PORT..."

# Check if service is running
if ! nc -z $HOST $PORT; then
    echo "ERROR: Service not responding on $HOST:$PORT"
    exit 1
fi

# Check health endpoint
HEALTH=$(curl -s http://$HOST:$PORT/health)
if [[ $HEALTH != *"OK"* ]]; then
    echo "WARNING: Health check failed"
    echo "$HEALTH"
fi

# Check resource usage
echo "Checking resources..."
ssh $HOST "ps aux | grep BulletHoleDetection"
ssh $HOST "du -sh /opt/bullet_detection"

# Check recent errors
echo "Recent errors:"
ssh $HOST "tail -50 logs/bullet_detection.log | grep ERROR"

echo "Health check complete"
```

---

## Monitoring & Alerts

### Key Metrics to Monitor

1. **Frame Processing Time**
   - Alert: > 100ms
   - Critical: > 500ms

2. **Memory Usage**
   - Alert: > 80% of limit
   - Critical: > 95% of limit

3. **Error Rate**
   - Alert: > 1% of operations
   - Critical: > 10% of operations

4. **Detection Count**
   - Alert: 0 detections in 5 minutes
   - Warning: < 5 detections/minute

5. **Service Availability**
   - Alert: Any downtime
   - Critical: > 5 minutes downtime

### Alert Escalation

```
Level 1 (Warning): Email to on-call engineer
Level 2 (Alert):   Page on-call engineer + email team
Level 3 (Critical): Page all engineers + email management + page on-call
```

---

## Performance Tuning (Production)

### CPU Optimization
```bash
# Set thread priority
nice -n -10 BulletHoleDetection

# Pin to specific cores
taskset -c 0,1,2,3 BulletHoleDetection
```

### Memory Optimization
```bash
# Use large pages
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# Disable memory swapping
swapoff -a
```

### I/O Optimization
```bash
# Use SSD for logs
ln -s /dev/sda1 /var/log/bullet_detection

# Enable write-back cache
echo "writethrough" > /sys/block/sda/cache_type
```

---

## Troubleshooting

### Service won't start

```bash
# Check logs
tail -100 /var/log/bullet_detection/app.log

# Check permissions
ls -la /opt/bullet_detection/
chmod +x /opt/bullet_detection/BulletHoleDetection

# Check dependencies
ldd /opt/bullet_detection/BulletHoleDetection

# Try manual start with debug output
/opt/bullet_detection/BulletHoleDetection --debug
```

### High CPU usage

```bash
# Check which thread is consuming CPU
top -H  # Show threads

# Profile with perf
perf record -p $(pidof BulletHoleDetection) -g -- sleep 60
perf report
```

### High memory usage

```bash
# Check memory map
pmap -x $(pidof BulletHoleDetection)

# Check for leaks
valgrind --leak-check=full --show-leak-kinds=all \
  /opt/bullet_detection/BulletHoleDetection
```

### Detection accuracy dropping

```bash
# Verify camera calibration
./verify_calibration.sh

# Check for noise in camera feed
./check_image_quality.sh

# Recalibrate if needed
./recalibrate_cameras.sh
```

---

## Maintenance Schedule

### Daily
- [ ] Check system logs for errors
- [ ] Verify service is running
- [ ] Monitor performance metrics
- [ ] Check disk space

### Weekly
- [ ] Review error statistics
- [ ] Update security patches
- [ ] Backup data
- [ ] Performance analysis

### Monthly
- [ ] Full system test
- [ ] Configuration review
- [ ] Capacity planning
- [ ] Security audit

### Quarterly
- [ ] Major version update testing
- [ ] Disaster recovery drill
- [ ] Performance benchmarking
- [ ] Architecture review

---

## Emergency Contacts

- **Primary On-Call**: [Name] - [Phone]
- **Secondary On-Call**: [Name] - [Phone]
- **Manager**: [Name] - [Phone]
- **CTO**: [Name] - [Phone]

---

## Post-Incident Review

After any production incident:

1. **Within 15 minutes**: Alert sent, on-call notified
2. **Within 1 hour**: Incident documented, preliminary assessment
3. **Within 24 hours**: Root cause analysis, fix implemented
4. **Within 3 days**: Post-mortem meeting, action items created
5. **Within 1 week**: Action items completed, changes deployed

---

## Version Information

- **Current Version**: 1.0.0
- **Build Date**: [Auto-generated]
- **Git Commit**: [Auto-generated]
- **Deployed By**: [Your name]
- **Deployment Date**: [Date]
- **Last Updated**: [Date]

