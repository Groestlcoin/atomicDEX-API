pipeline {
  agent any
  stages {
    stage('Prepare') {
      steps {
        sh '''cp -r /root/.env.client .env.client
cp -r /root/.env.seed .env.seed'''
      }
    }
    stage('Build') {
      steps {
        sh '''cargo build --bin mm2-nop --features nop
cargo build -vv
cargo test
cargo test --package etomicrs
docker-compose build'''
      }
    }
    stage('Trade BEER/ETH') {
      steps {
        sh '''docker-compose up -d
./start_BEER_OTHER_trade.sh ETH
timeout 600 grep -q "SWAP completed" <(COMPOSE_HTTP_TIMEOUT=600 docker-compose logs -f clientnode)
timeout 600 grep -q "SWAP completed" <(COMPOSE_HTTP_TIMEOUT=600 docker-compose logs -f seednode)'''
      }
      post {
        always {
          sh '''docker-compose logs --timestamps --tail=999
docker ps
docker-compose down'''
        }
      }
    }
    stage('Trade ETH/BEER') {
      steps {
        sh '''docker-compose up -d
./start_BEER_OTHER_trade_inverted.sh ETH
timeout 600 grep -q "SWAP completed" <(COMPOSE_HTTP_TIMEOUT=600 docker-compose logs -f clientnode)
timeout 600 grep -q "SWAP completed" <(COMPOSE_HTTP_TIMEOUT=600 docker-compose logs -f seednode)'''
      }
      post {
        always {
          sh '''docker-compose logs --timestamps --tail=999
docker ps
docker-compose down'''
        }
      }
    }
  }
}