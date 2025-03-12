#readme
source esp32/esp-idf/export.sh
idf.py create-project remoteLED
echo "# remoteLED" >> README.md
git init
git add README.md
git commit -m "first commit"
git branch -M main
git remote add origin https://github.com/pdhrutaraj/remoteLED.git
git push -u origin main


