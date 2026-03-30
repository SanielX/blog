
console.log("Hello world");

{
  const images  = document.querySelectorAll(".expandable");
  const overlay = document.getElementById("overlay");
  const overlayImg = overlay.querySelector("img");

  images.forEach(img => {
    img.onclick = () => {
      overlay.style.display = "flex";
      if(img.nodeName == "FIGURE")
      { img = img.querySelector("img"); }
      overlayImg.src = img.src;
    };
  });

  overlay.onclick = () => overlay.style.display = "none";
}