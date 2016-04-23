function syntaxHighlight(json) {
    if (typeof json != 'string') {
        json = JSON.stringify(json, undefined, 2);
    }
    json = json.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return '<pre><code class="json">' + json + '</code></pre>';
}

$.ajax({
    url: '/control/version',
    //        data:{"id":id},
    type: 'GET',

    success: function(data) {
        html = "<center>" + data['aura_version'] + " (" + data['aura_versioncode'] + ")" + "</center>"
        $('.version').replaceWith(html);
    }
});


function getAndDisplayJSON(uri)
{
    $.ajax({
        url: uri,
        type: 'GET',

        success: function(data) {
            $('.ui-content').html(syntaxHighlight(data));
            $('pre code').each(function(i, block) {
                hljs.highlightBlock(block);
            });
        }
    });
}

function showNode(item)
{
    if (item.type == "control")
        getAndDisplayJSON(item.mountpoint + "/fstab")
    if (item.type == "node")
        getAndDisplayJSON(item.mountpoint + "/exports")
    if (item.type == "static")
        $(".ui-content").html("<iframe width=100% height=100% border=none src='" + item.mountpoint+ "/'></iframe>")

}

function loadFstab()
{

}

$.ajax({
    url: "/control/fstab",
    type: 'GET',

    success: function(data) {
        $(".mountpoints").html("");
        data["fstab"].forEach(function(item, i, arr) {
            var newli = $("<li>").append($("<a href=\"#\">")
            .append(item.mountpoint)
            .click(function () {
                showNode(item)
            }));
            $(".mountpoints").append(newli);
        });

    }
});
