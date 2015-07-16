cp data/export_fwk_model.py temp_script$$.py
echo -e "\nwrite(\"$2\", [])" >> temp_script$$.py
blender $1 --background --python temp_script$$.py
rm temp_script$$.py
