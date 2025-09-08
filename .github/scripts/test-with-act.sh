#!/bin/bash
# Test GitHub Actions workflow locally with act
# Copyright 2025 superstruct ltd, New Zealand

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if act is installed
check_act_installation() {
    if ! command -v act &> /dev/null; then
        log_error "act is not installed. Please install it first:"
        echo "  brew install act  # macOS"
        echo "  # or"
        echo "  curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash  # Linux"
        exit 1
    fi
    
    log_info "act version: $(act --version)"
}

# Check if Docker is running
check_docker() {
    if ! docker info &> /dev/null; then
        log_error "Docker is not running. Please start Docker first."
        exit 1
    fi
    
    log_info "Docker is running"
}

# Setup act secrets for local testing
setup_act_secrets() {
    local secrets_dir="${PROJECT_ROOT}/.github/secrets"
    local secrets_file="${secrets_dir}/act-secrets"
    
    mkdir -p "${secrets_dir}"
    
    if [[ ! -f "${secrets_file}" ]]; then
        log_info "Creating act secrets file for local testing..."
        cat > "${secrets_file}" <<EOF
# Act secrets for local testing
# These are fake secrets for development only
GITHUB_TOKEN=ghp_fake_token_for_local_testing
NODE_AUTH_TOKEN=npm_fake_token_for_local_testing
EOF
        log_warning "Created fake secrets file at ${secrets_file}"
        log_warning "Do not commit this file or use real secrets!"
    fi
}

# Pull required Docker images
pull_docker_images() {
    log_info "Pulling required Docker images..."
    
    # Ubuntu image for GitHub Actions
    docker pull catthehacker/ubuntu:act-20.04
    
    log_success "Docker images ready"
}

# Test specific workflow job
test_workflow_job() {
    local job_name="${1:-build-variants}"
    local workflow_file="${2:-.github/workflows/wasm-build.yml}"
    
    log_info "Testing workflow job: ${job_name}"
    
    cd "${PROJECT_ROOT}"
    
    # Run act with specific job
    if act -j "${job_name}" \
        --workflows "${workflow_file}" \
        --artifact-server-path /tmp/act-artifacts \
        --env-file .env.local \
        --verbose; then
        log_success "Job '${job_name}' completed successfully"
        return 0
    else
        log_error "Job '${job_name}' failed"
        return 1
    fi
}

# Test workflow with matrix strategy
test_matrix_job() {
    local matrix_include="${1}"
    
    log_info "Testing matrix job with: ${matrix_include}"
    
    cd "${PROJECT_ROOT}"
    
    # Set matrix environment variables
    export MATRIX_VARIANT=$(echo "${matrix_include}" | jq -r '.variant // "release"')
    export MATRIX_CMAKE_FLAGS=$(echo "${matrix_include}" | jq -r '.cmake_flags // ""')
    export MATRIX_TEST_FLAGS=$(echo "${matrix_include}" | jq -r '.test_flags // ""')
    
    log_info "Matrix variant: ${MATRIX_VARIANT}"
    
    if act -j build-variants \
        --matrix "${matrix_include}" \
        --artifact-server-path /tmp/act-artifacts \
        --env MATRIX_VARIANT="${MATRIX_VARIANT}" \
        --env MATRIX_CMAKE_FLAGS="${MATRIX_CMAKE_FLAGS}" \
        --env MATRIX_TEST_FLAGS="${MATRIX_TEST_FLAGS}" \
        --verbose; then
        log_success "Matrix job completed successfully"
        return 0
    else
        log_error "Matrix job failed"
        return 1
    fi
}

# Run full workflow validation
validate_workflow() {
    local workflow_file=".github/workflows/wasm-build.yml"
    
    log_info "Validating workflow syntax..."
    
    # Check if workflow file exists
    if [[ ! -f "${workflow_file}" ]]; then
        log_error "Workflow file not found: ${workflow_file}"
        return 1
    fi
    
    # Validate YAML syntax
    if command -v yq &> /dev/null; then
        if yq eval . "${workflow_file}" > /dev/null; then
            log_success "Workflow YAML syntax is valid"
        else
            log_error "Invalid YAML syntax in workflow file"
            return 1
        fi
    else
        log_warning "yq not found, skipping YAML validation"
    fi
    
    # List workflow jobs
    log_info "Workflow jobs:"
    if command -v yq &> /dev/null; then
        yq eval '.jobs | keys | .[]' "${workflow_file}" | sed 's/^/  - /'
    else
        grep -E "^  [a-zA-Z0-9_-]+:" "${workflow_file}" | sed 's/://g' | sed 's/^/  - /'
    fi
    
    return 0
}

# Generate test report
generate_test_report() {
    local artifacts_dir="/tmp/act-artifacts"
    local report_file="${PROJECT_ROOT}/act-test-report.md"
    
    log_info "Generating test report..."
    
    cat > "${report_file}" <<EOF
# Act Local Testing Report

**Date:** $(date)
**Commit:** $(git rev-parse --short HEAD 2>/dev/null || echo "unknown")

## Test Summary

EOF
    
    if [[ -d "${artifacts_dir}" ]]; then
        echo "## Artifacts Generated" >> "${report_file}"
        find "${artifacts_dir}" -type f -name "*.log" -o -name "*.json" -o -name "*.txt" | \
        while read -r file; do
            echo "- $(basename "${file}")" >> "${report_file}"
        done
        echo "" >> "${report_file}"
    fi
    
    echo "## Docker Images Used" >> "${report_file}"
    docker images --filter "reference=catthehacker/*" --format "table {{.Repository}}:{{.Tag}}\t{{.Size}}" >> "${report_file}"
    
    log_success "Test report generated: ${report_file}"
}

# Clean up after testing
cleanup() {
    log_info "Cleaning up..."
    
    # Remove temporary artifacts
    rm -rf /tmp/act-artifacts
    
    # Clean up Docker containers
    docker container prune -f --filter "label=act" 2>/dev/null || true
    
    log_success "Cleanup completed"
}

# Main function
main() {
    local command="${1:-help}"
    shift || true
    
    case "${command}" in
        "check")
            check_act_installation
            check_docker
            log_success "Environment is ready for act testing"
            ;;
        "setup")
            check_act_installation
            check_docker
            setup_act_secrets
            pull_docker_images
            log_success "Act testing environment setup completed"
            ;;
        "validate")
            validate_workflow
            ;;
        "test-job")
            local job_name="${1:-build-variants}"
            check_act_installation
            check_docker
            setup_act_secrets
            test_workflow_job "${job_name}"
            ;;
        "test-matrix")
            local variant="${1:-release}"
            local matrix_json="{\"variant\":\"${variant}\",\"cmake_flags\":\"-DCMAKE_BUILD_TYPE=Release\",\"test_flags\":\"\"}"
            check_act_installation
            check_docker
            setup_act_secrets
            test_matrix_job "${matrix_json}"
            ;;
        "full-test")
            check_act_installation
            check_docker
            setup_act_secrets
            validate_workflow
            
            # Test key build variants
            for variant in release debug simd; do
                log_info "Testing variant: ${variant}"
                local matrix_json="{\"variant\":\"${variant}\",\"cmake_flags\":\"-DCMAKE_BUILD_TYPE=Release\",\"test_flags\":\"\"}"
                test_matrix_job "${matrix_json}" || log_error "Variant ${variant} failed"
            done
            
            generate_test_report
            ;;
        "report")
            generate_test_report
            ;;
        "clean")
            cleanup
            ;;
        "help"|*)
            cat <<EOF
Usage: $0 <command> [arguments]

Commands:
    check           Check if act and Docker are properly installed
    setup           Setup act testing environment (pull images, create secrets)
    validate        Validate workflow YAML syntax and structure
    test-job <job>  Test a specific workflow job (default: build-variants)
    test-matrix <variant>  Test matrix job with specific variant (default: release)
    full-test       Run comprehensive test of all key variants
    report          Generate test report
    clean           Clean up temporary files and Docker containers
    help            Show this help message

Examples:
    $0 setup                    # Initial setup
    $0 validate                 # Check workflow syntax
    $0 test-job build-variants  # Test main build job
    $0 test-matrix simd         # Test SIMD variant
    $0 full-test                # Run comprehensive tests
    $0 clean                    # Cleanup

Prerequisites:
    - act: https://github.com/nektos/act
    - Docker Desktop or Docker Engine
    - jq (optional, for matrix testing)
    - yq (optional, for YAML validation)
EOF
            ;;
    esac
}

# Set up trap for cleanup on exit
trap cleanup EXIT

# Run main function with all arguments
main "$@"