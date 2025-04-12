:: filepath: c:\projekty\bimbrownik\automatical_tests\setup_env_and_run.bat
@echo off
echo Setting up Python virtual environment...

:: Create virtual environment if it doesn't exist
if not exist venv (
    python -m venv venv
)

:: Activate virtual environment
call venv\Scripts\activate

:: Install dependencies
pip install -r requirements.txt
