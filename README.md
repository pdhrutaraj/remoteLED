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

echo -n | openssl s_client -connect eapi-vijn.onrender.com:443 | \
sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' > cert.pem

mv cert.pem ~/esp32/remoteLED/main/
2️⃣ Modify CMakeLists.txt in main/
cmake
Copy
Edit
idf_component_register(SRCS "remoteLED.c"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES cert.pem)

