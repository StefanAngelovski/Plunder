# 🏴‍☠️ Plunder  

**A romsite scraper and frontend for the Trimui Smart Pro handheld.**  

Plunder makes it easy to browse, search, and download games directly from your device.  
It works on both the **stock Trimui OS** and **[CrossMix OS](https://github.com/cizia64/CrossMix-OS)**.  

---

## 🚀 Installation  

1. Download the latest release from [Releases](https://github.com/StefanAngelovski/Plunder/releases).  
2. Unzip the archive.  
3. Place the **Plunder** folder into:  


4. Launch it from your Trimui Smart Pro app menu.  

---

## 🎮 Usage  

- **Browse & Scrape:** Explore several popular romsites.  
- **Search & Filter:** Quickly find your favorite games with filtering.  
- **Auto-mapped Consoles:** Scraped games are mapped directly to your device’s folder structure.  
- **Cache Management:** A single button clears the image cache for smoother performance.  

📸 **Preview:**  
<img width="1719" height="1080" alt="UI Screenshot" src="https://github.com/user-attachments/assets/6061aab7-85ca-4c11-9af1-687a1d859faa" />  
🎥 **Preview:**

[**Watch the demo video**](https://github.com/user-attachments/assets/903e35f0-b2c2-46d3-8dd1-a1b69d0ae6be)  

🖼️ **App logo in Trimui menu:**  
<img width="1920" height="1079" alt="Logo" src="https://github.com/user-attachments/assets/cb8dae6a-80de-474a-9b51-248938ad448b" />  

---

## ✨ Features  

- Works with **stock OS** and **CrossMix OS**  
- Modular scraper design → easily expandable to more romsites  
- Regex-powered scraping (with planned improvements for readability)  
- Filtering system for faster game discovery  
- One-click **cache clearing** for optimization  
- Direct console ↔ folder mapping  

---

## 🔮 Roadmap / Known Issues  

- [ ] **Auto-updating** for seamless releases  
- [ ] Improve modularity of scrapers & reduce repeated logic  
- [ ] Replace regex-based parsing with a cleaner library  
- [ ] Fix missing images due to `SDL2_image` lacking WebP support in CrossMix  

---

## ⚙️ Developer Notes  

Development of Plunder was made possible thanks to the excellent **[Trimui Smart Pro Docker Build System by Maxwell](https://github.com/Maxwell-SS/trimui-smart-pro-build-system)**.  
Please consult his documentation if you’d like to build Plunder from source.  

---

## 🏴 Credits  

- **Main Developer:** [@StefanAngelovski](https://github.com/StefanAngelovski)  
- **Build System:** [@Maxwell-SS](https://github.com/Maxwell-SS)  
