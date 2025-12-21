const express = require("express");
const axios = require("axios");
const sharp = require("sharp"); // npm install sharp

const app = express();

app.get("/album", async (req, res) => {
  const url = req.query.url;
  const size = 240; // default 240x240

  try {
    // download image
    const img = await axios.get(url, { responseType: "arraybuffer" });

    // resize pakai sharp
    const resized = await sharp(img.data)
      .resize(size, size)   
      .jpeg()               
      .toBuffer();

    res.set("Content-Type", "image/jpeg");
    res.send(resized);
  } catch (err) {
    console.error(err);
    res.status(500).send("Error fetching or resizing image");
  }
});

app.listen(3000, () => console.log("Proxy running on port 3000"));
